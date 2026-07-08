#include "can_driver.h"

#include "fsy_dispatch.h"
#include "fsy_frame.h"
#include "fsy_link.h"
#include "main.h"
#include "uart_diag.h"

#include <string.h>
#include <stdio.h>

#if !CAN_DRIVER_ENABLE

bool CanDriver_Init(void)
{
    UartDiag_Write("[CAN] disabled (CAN_DRIVER_ENABLE=0)\r\n");
    return false;
}

bool CanDriver_IsReady(void)
{
    return false;
}

bool CanDriver_TransmitStd(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
    (void)std_id;
    (void)data;
    (void)dlc;
    return false;
}

bool CanDriver_TransmitRtu(const uint8_t *frame, uint16_t len)
{
    (void)frame;
    (void)len;
    return false;
}

bool CanDriver_RxPop(CanRxItem *out)
{
    (void)out;
    return false;
}

void CanDriver_Poll(void)
{
}

#else /* CAN_DRIVER_ENABLE */

/* -------------------------------------------------------------------------- */
/*  与 zjb can.c / RAD-I fdcan.c 对齐：Classic CAN @ 500 kbps                  */
/*  FDCAN 时钟 120 MHz：120M / (15 * (1+13+2)) = 500 kbps                     */
/* -------------------------------------------------------------------------- */

#define CAN_TX_SLICE_GAP_MS       1U
#define CAN_RX_CACHE_SIZE         FSY_FRAME_MAX_LEN
#define CAN_RX_MAX_ADDR_SLOTS     2U
#define CAN_RX_STALE_TIMEOUT_MS   200U
#define CAN_POLL_MAX_LOOPS        128U

typedef struct {
    uint8_t  addr;
    uint8_t  buf[CAN_RX_CACHE_SIZE];
    uint16_t len;
    uint32_t last_tick_ms;
} CanRtuCache;

static FDCAN_HandleTypeDef s_hfdcan2;
static volatile uint8_t    s_ready;

static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static CanRxItem         s_rx_q[CAN_RX_QUEUE_SIZE];
static CanRtuCache       s_rtu_cache[CAN_RX_MAX_ADDR_SLOTS];

static uint8_t can_fdcan_dlc_bytes(uint32_t fdcan_dlc)
{
    if (fdcan_dlc <= FDCAN_DLC_BYTES_8) {
        return (uint8_t)fdcan_dlc;
    }
    return 8U;
}

static CanRtuCache *can_cache_get_or_alloc(uint8_t addr)
{
    uint8_t i;
    CanRtuCache *free_slot = NULL;

    for (i = 0U; i < CAN_RX_MAX_ADDR_SLOTS; i++) {
        if (s_rtu_cache[i].addr == addr) {
            return &s_rtu_cache[i];
        }
        if (s_rtu_cache[i].addr == 0U && free_slot == NULL) {
            free_slot = &s_rtu_cache[i];
        }
    }

    if (free_slot != NULL) {
        free_slot->addr = addr;
        free_slot->len = 0U;
        free_slot->last_tick_ms = HAL_GetTick();
        return free_slot;
    }

    {
        uint8_t oldest = 0U;
        uint32_t oldest_tick = s_rtu_cache[0].last_tick_ms;

        for (i = 1U; i < CAN_RX_MAX_ADDR_SLOTS; i++) {
            if (s_rtu_cache[i].last_tick_ms < oldest_tick) {
                oldest_tick = s_rtu_cache[i].last_tick_ms;
                oldest = i;
            }
        }
        s_rtu_cache[oldest].addr = addr;
        s_rtu_cache[oldest].len = 0U;
        s_rtu_cache[oldest].last_tick_ms = HAL_GetTick();
        return &s_rtu_cache[oldest];
    }
}

static void can_cache_append(CanRtuCache *cache, const uint8_t *data, uint8_t dlc,
                             uint32_t tick_ms)
{
    if ((cache == NULL) || (data == NULL) || (dlc == 0U)) {
        return;
    }

    cache->last_tick_ms = tick_ms;

    if ((uint32_t)cache->len + (uint32_t)dlc > (uint32_t)sizeof(cache->buf)) {
        cache->len = 0U;
    }
    memcpy(&cache->buf[cache->len], data, dlc);
    cache->len = (uint16_t)(cache->len + dlc);
}

static void can_cache_clear_stale(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t i;

    for (i = 0U; i < CAN_RX_MAX_ADDR_SLOTS; i++) {
        if (s_rtu_cache[i].addr != 0U && s_rtu_cache[i].len != 0U) {
            if ((now - s_rtu_cache[i].last_tick_ms) > CAN_RX_STALE_TIMEOUT_MS) {
                s_rtu_cache[i].len = 0U;
            }
        }
    }
}

static void can_cache_try_dispatch(CanRtuCache *cache)
{
    uint8_t resp[FSY_FRAME_MAX_LEN];
    int resp_len;

    for (;;) {
        uint16_t frame_len = Fsy_Frame_RtuAssembleLen(cache->buf, cache->len);

        if (frame_len == FSY_FRAME_LEN_NEED_MORE) {
            return;
        }
        if (frame_len == 0U) {
            if (cache->len < 1U) {
                return;
            }
            memmove(cache->buf, &cache->buf[1], cache->len - 1U);
            cache->len--;
            continue;
        }
        if (frame_len > (uint16_t)sizeof(cache->buf)) {
            memmove(cache->buf, &cache->buf[1], cache->len - 1U);
            cache->len--;
            continue;
        }
        if (cache->len < frame_len) {
            return;
        }

        if (!Fsy_Frame_CrcOk(cache->buf, frame_len)) {
            memmove(cache->buf, &cache->buf[1], cache->len - 1U);
            cache->len--;
            continue;
        }

        resp_len = Fsy_Dispatch_Request(cache->buf, frame_len, resp, sizeof(resp));
        if (resp_len > 0) {
            (void)Fsy_Link_WriteUart(resp, (uint16_t)resp_len);
        }

        if (cache->len > frame_len) {
            memmove(cache->buf, &cache->buf[frame_len], cache->len - frame_len);
            cache->len = (uint16_t)(cache->len - frame_len);
        } else {
            cache->len = 0U;
            return;
        }
    }
}

static bool can_config_fdcan_clock(void)
{
    RCC_PeriphCLKInitTypeDef cfg = {0};

    cfg.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    cfg.PLL2.PLL2M = 5;
    cfg.PLL2.PLL2N = 192;
    cfg.PLL2.PLL2P = 10;
    cfg.PLL2.PLL2Q = 8;
    cfg.PLL2.PLL2R = 8;
    cfg.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    cfg.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    cfg.PLL2.PLL2FRACN = 0;
    cfg.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL2;

    if (HAL_RCCEx_PeriphCLKConfig(&cfg) != HAL_OK) {
        return false;
    }
    return true;
}

static bool can_config_filters(void)
{
    FDCAN_FilterTypeDef filter = {0};

    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0U;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = 0x000U;
    filter.FilterID2 = 0x000U;
    if (HAL_FDCAN_ConfigFilter(&s_hfdcan2, &filter) != HAL_OK) {
        return false;
    }

    if (HAL_FDCAN_ConfigGlobalFilter(
            &s_hfdcan2,
            FDCAN_ACCEPT_IN_RX_FIFO0,
            FDCAN_REJECT,
            FDCAN_FILTER_REMOTE,
            FDCAN_FILTER_REMOTE) != HAL_OK) {
        return false;
    }

    return true;
}

static uint32_t can_dlc_code(uint8_t len)
{
    switch (len) {
    case 0: return FDCAN_DLC_BYTES_0;
    case 1: return FDCAN_DLC_BYTES_1;
    case 2: return FDCAN_DLC_BYTES_2;
    case 3: return FDCAN_DLC_BYTES_3;
    case 4: return FDCAN_DLC_BYTES_4;
    case 5: return FDCAN_DLC_BYTES_5;
    case 6: return FDCAN_DLC_BYTES_6;
    case 7: return FDCAN_DLC_BYTES_7;
    default: return FDCAN_DLC_BYTES_8;
    }
}

static bool can_hw_init(void)
{
    s_hfdcan2.Instance = FDCAN2;
    s_hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    s_hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
    /* 与 zjb can.c 一致：无 ACK 时不无限重发，避免未接线时 TX FIFO 堆满 / Bus-Off */
    s_hfdcan2.Init.AutoRetransmission = DISABLE;
    s_hfdcan2.Init.TransmitPause = DISABLE;
    s_hfdcan2.Init.ProtocolException = DISABLE;
    s_hfdcan2.Init.NominalPrescaler = 15;
    s_hfdcan2.Init.NominalSyncJumpWidth = 2;
    s_hfdcan2.Init.NominalTimeSeg1 = 13;
    s_hfdcan2.Init.NominalTimeSeg2 = 2;
    s_hfdcan2.Init.DataPrescaler = 15;
    s_hfdcan2.Init.DataSyncJumpWidth = 2;
    s_hfdcan2.Init.DataTimeSeg1 = 13;
    s_hfdcan2.Init.DataTimeSeg2 = 2;
    s_hfdcan2.Init.MessageRAMOffset = 0;
    s_hfdcan2.Init.StdFiltersNbr = 1;
    s_hfdcan2.Init.ExtFiltersNbr = 0;
    s_hfdcan2.Init.RxFifo0ElmtsNbr = 32;
    s_hfdcan2.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    s_hfdcan2.Init.RxFifo1ElmtsNbr = 0;
    s_hfdcan2.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
    s_hfdcan2.Init.RxBuffersNbr = 0;
    s_hfdcan2.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    s_hfdcan2.Init.TxEventsNbr = 0;
    s_hfdcan2.Init.TxBuffersNbr = 0;
    s_hfdcan2.Init.TxFifoQueueElmtsNbr = 32;
    s_hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    s_hfdcan2.Init.TxElmtSize = FDCAN_DATA_BYTES_8;

    if (HAL_FDCAN_Init(&s_hfdcan2) != HAL_OK) {
        return false;
    }
    if (!can_config_filters()) {
        return false;
    }
    if (HAL_FDCAN_Start(&s_hfdcan2) != HAL_OK) {
        return false;
    }
    if (HAL_FDCAN_ActivateNotification(&s_hfdcan2, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0U) != HAL_OK) {
        return false;
    }
    return true;
}

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef *fdcanHandle)
{
    GPIO_InitTypeDef gpio = {0};

    if (fdcanHandle->Instance != FDCAN2) {
        return;
    }

    __HAL_RCC_FDCAN_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_NVIC_SetPriority(FDCAN2_IT0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
}

void HAL_FDCAN_MspDeInit(FDCAN_HandleTypeDef *fdcanHandle)
{
    if (fdcanHandle->Instance != FDCAN2) {
        return;
    }

    __HAL_RCC_FDCAN_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12 | GPIO_PIN_13);
    HAL_NVIC_DisableIRQ(FDCAN2_IT0_IRQn);
}

static bool can_rx_push(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
    uint16_t next = (uint16_t)((s_rx_head + 1U) % CAN_RX_QUEUE_SIZE);

    if (next == s_rx_tail) {
        return false;
    }

    s_rx_q[s_rx_head].std_id = std_id;
    s_rx_q[s_rx_head].dlc = dlc;
    memset(s_rx_q[s_rx_head].data, 0, sizeof(s_rx_q[s_rx_head].data));
    if (dlc > 0U) {
        memcpy(s_rx_q[s_rx_head].data, data, dlc);
    }
    s_rx_q[s_rx_head].tick_ms = HAL_GetTick();
    s_rx_head = next;
    return true;
}

void FDCAN2_IT0_IRQHandler(void)
{
    HAL_FDCAN_IRQHandler(&s_hfdcan2);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t rx_fifo0_its)
{
    FDCAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    if ((hfdcan != &s_hfdcan2) ||
        ((rx_fifo0_its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U)) {
        return;
    }

    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U) {
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, data) != HAL_OK) {
            break;
        }
        if (hdr.IdType != FDCAN_STANDARD_ID) {
            continue;
        }
        if (hdr.RxFrameType != FDCAN_DATA_FRAME) {
            continue;
        }
        if (hdr.DataLength > FDCAN_DLC_BYTES_8) {
            continue;
        }
        (void)can_rx_push((uint16_t)hdr.Identifier, data,
                          can_fdcan_dlc_bytes(hdr.DataLength));
    }
}

bool CanDriver_Init(void)
{
    char msg[72];

    s_ready = 0U;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    memset(s_rtu_cache, 0, sizeof(s_rtu_cache));

    if (!can_config_fdcan_clock()) {
        UartDiag_Write("[CAN] FDCAN clock config fail\r\n");
        return false;
    }
    if (!can_hw_init()) {
        UartDiag_Write("[CAN] FDCAN2 init fail\r\n");
        return false;
    }

    s_ready = 1U;
    (void)snprintf(msg, sizeof(msg),
                   "[CAN] FDCAN2 ready Classic 500kbps PB12/PB13\r\n");
    UartDiag_Write(msg);
    return true;
}

bool CanDriver_IsReady(void)
{
    return (s_ready != 0U);
}

bool CanDriver_TransmitStd(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
    FDCAN_TxHeaderTypeDef tx = {0};
    uint8_t payload[8];

    if ((s_ready == 0U) || (data == NULL) || (dlc == 0U) || (dlc > 8U)) {
        return false;
    }

    memset(payload, 0, sizeof(payload));
    memcpy(payload, data, dlc);

    tx.Identifier = (uint32_t)std_id;
    tx.IdType = FDCAN_STANDARD_ID;
    tx.TxFrameType = FDCAN_DATA_FRAME;
    tx.DataLength = can_dlc_code(dlc);
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch = FDCAN_BRS_OFF;
    tx.FDFormat = FDCAN_CLASSIC_CAN;
    tx.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    tx.MessageMarker = 0U;

    return (HAL_FDCAN_AddMessageToTxFifoQ(&s_hfdcan2, &tx, payload) == HAL_OK);
}

bool CanDriver_TransmitRtu(const uint8_t *frame, uint16_t len)
{
    uint16_t offset = 0U;
    uint16_t std_id;

    if ((s_ready == 0U) || (frame == NULL) || (len < 1U)) {
        return false;
    }

    std_id = (uint16_t)frame[0];

    while (offset < len) {
        uint8_t chunk = (uint8_t)(((len - offset) > 8U) ? 8U : (len - offset));

        if (!CanDriver_TransmitStd(std_id, &frame[offset], chunk)) {
            return false;
        }
        offset = (uint16_t)(offset + chunk);
        if (offset < len) {
            HAL_Delay(CAN_TX_SLICE_GAP_MS);
        }
    }
    return true;
}

bool CanDriver_RxPop(CanRxItem *out)
{
    CanRxItem item;

    if ((out == NULL) || (s_rx_tail == s_rx_head)) {
        return false;
    }

    item = s_rx_q[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) % CAN_RX_QUEUE_SIZE);
    *out = item;
    return true;
}

void CanDriver_Poll(void)
{
    CanRxItem item;
    uint16_t loops = 0U;
    uint8_t my_addr = Fsy_Dispatch_GetDeviceAddr();

    if (s_ready == 0U) {
        return;
    }

    can_cache_clear_stale();

    while (CanDriver_RxPop(&item)) {
        uint8_t addr;

        if (item.dlc == 0U) {
            continue;
        }

        /* 只处理发给本机 Modbus 地址的 CAN 帧（StdId = RTU addr） */
        addr = (uint8_t)(item.std_id & 0xFFU);
        if (addr != my_addr) {
            continue;
        }

        {
            CanRtuCache *cache = can_cache_get_or_alloc(addr);
            can_cache_append(cache, item.data, item.dlc, item.tick_ms);
            can_cache_try_dispatch(cache);
        }

        loops++;
        if (loops >= CAN_POLL_MAX_LOOPS) {
            break;
        }
    }
}

#endif /* CAN_DRIVER_ENABLE */
