#include "lora.h"
#include "usart.h"
#include "geiger.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>

static SemaphoreHandle_t s_lora_uart_mtx = NULL;
static volatile uint8_t s_lora_cfg_active = 0U;
static volatile uint8_t s_lora_cfg_internal = 0U;

#if LORA_UART_RX_IRQ
static volatile uint8_t s_lora_rx_ring[LORA_UART_RX_RING_SIZE];
static volatile uint16_t s_lora_rx_head = 0U;
static volatile uint16_t s_lora_rx_tail = 0U;
static volatile uint8_t s_lora_rx_irq_en = 0U;
static volatile uint32_t s_lora_rx_drop = 0U;
#endif

#if LORA_DEBUG
#define LORA_DBG(...)   printf("[LORA_DBG] " __VA_ARGS__)
static void lora_dbg_hex(const char *tag, const uint8_t *data, uint16_t len);
static void lora_dbg_pins(void);
static void lora_dbg_uart_flags(void);
#else
#define LORA_DBG(...)   ((void)0)
#endif

static void lora_delay_ms(uint32_t ms);
static bool lora_wait_aux_ready(uint32_t timeout_ms);
static bool lora_wait_after_mode_change(uint32_t timeout_ms);
static bool lora_uart_poll_byte(uint8_t *b, bool *from_ore);
static bool lora_is_cfg_rsp_head(uint8_t b);
static bool lora_set_mode(bool config_mode);
static bool lora_parse_config_frame(const uint8_t *frame, uint16_t len, LORA_Config_t *config);
static void lora_uart_state_reset(void);
static void lora_uart_rx_flush(void);
static void lora_uart_drain_rx(uint8_t *buf, uint16_t *got, uint16_t cap);
static void lora_uart_push_cfg_byte(uint8_t *buf, uint16_t *got, uint16_t cap, uint8_t b,
                                    uint16_t *discard);
static uint16_t lora_uart_read_frame(uint8_t *buf, uint16_t cap, uint16_t expect_len,
                                     uint32_t timeout_ms);
static uint16_t lora_uart_read_cfg_rsp(uint8_t *buf, uint16_t cap, uint32_t timeout_ms);
static void lora_default_config(LORA_Config_t *config);
static bool lora_config_read(LORA_Config_t *config);
static bool lora_config_write(const LORA_Config_t *config, uint8_t head);
//static bool lora_sync_param(LORA_Param_t id, uint32_t target, const char *tag);
static void lora_uart_mutex_init(void);
static bool lora_uart_transmit_raw(uint8_t *pdata, uint16_t len);
static bool lora_cfg_session_begin(void);
static void lora_cfg_session_end(void);
#if LORA_UART_RX_IRQ
static void lora_uart_rx_ring_reset(void);
static bool lora_uart_rx_ring_push(uint8_t b);
static bool lora_uart_rx_ring_pop(uint8_t *b);
static void lora_uart_hw_drain(void);
static bool lora_uart_hw_poll_byte(uint8_t *b, bool *from_ore);
static void lora_uart_rx_irq_enable(void);
static void lora_uart_rx_irq_disable(void);
#endif

/*===========================================================================
 * 底层驱动接口（UART5）：Init / Transmit / Receive
 *===========================================================================*/

bool LORA_Init(void)
{
    bool ok;

    if(huart5.Instance == NULL)
    {
        LORA_DBG("Init 失败: huart5 未初始化\r\n");
        return false;
    }

    LORA_DBG("Init: 进入正常模式 M0=0 M1=0\r\n");
    HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_RESET);
    lora_delay_ms(30);
#if LORA_DEBUG
    lora_dbg_pins();
#endif
    ok = lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS);
    LORA_DBG("Init %s (AUX 就绪)\r\n", ok ? "完成" : "失败");
#if LORA_UART_RX_IRQ
    if(ok)
    {
        lora_uart_rx_ring_reset();
        lora_uart_rx_irq_enable();
    }
#endif
    return ok;
}

bool LORA_Transmit(uint8_t *pdata, uint16_t len)
{
    bool ok;

    if(pdata == NULL || len == 0)
        return false;

    /* 配置模式会话中：协议层不得向 LoRa 发数据（内部配置走 lora_uart_transmit_raw） */
    if(s_lora_cfg_active)
        return false;

    lora_uart_mutex_init();
    if(s_lora_uart_mtx != NULL &&
       xSemaphoreTake(s_lora_uart_mtx, pdMS_TO_TICKS(500)) != pdTRUE)
        return false;

    if(!lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS))
    {
        if(s_lora_uart_mtx != NULL)
            (void)xSemaphoreGive(s_lora_uart_mtx);
        return false;
    }

    ok = lora_uart_transmit_raw(pdata, len);
    if(ok)
        ok = lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS);

    if(s_lora_uart_mtx != NULL)
        (void)xSemaphoreGive(s_lora_uart_mtx);
    return ok;
}

uint16_t LORA_Receive(uint8_t *pdata, uint16_t expect_len, uint32_t timeout_ms)
{
    uint16_t n = 0;

    if(pdata == NULL)
        return 0;

    /* 配置模式：UART 由配置会话独占（raw 轮询），协议层不得读 FIFO */
    if(s_lora_cfg_active)
        return 0;

    lora_uart_mutex_init();
    if(s_lora_uart_mtx != NULL &&
        xSemaphoreTake(s_lora_uart_mtx, pdMS_TO_TICKS(timeout_ms ? timeout_ms : 20U)) != pdTRUE)
        return 0;

    if(expect_len == 0U)
        n = lora_uart_read_frame(pdata, LORA_REC_LEN, 0, 0);
    else if(timeout_ms != 0U)
        n = lora_uart_read_frame(pdata, LORA_REC_LEN, expect_len, timeout_ms);

    if(s_lora_uart_mtx != NULL)
        (void)xSemaphoreGive(s_lora_uart_mtx);
    return n;
}

bool LORA_IsCfgBusy(void)
{
    return (s_lora_cfg_active != 0U);
}

/*===========================================================================
 * 配置调度
 *===========================================================================*/

bool LORA_Config(LORA_CfgOp_t op, LORA_Config_t *cfg)
{
//    static const char *op_name[] = {"READ", "WRITE", "WRITE_VOL", "DEFAULT"};

    if((unsigned)op < 4U)
        LORA_DBG("Config 操作: %s\r\n", op_name[op]);

    switch(op)
    {
    case LORA_CFG_READ:
        return lora_config_read(cfg);
    case LORA_CFG_WRITE:
        return lora_config_write(cfg, 0xC0);
    case LORA_CFG_WRITE_VOLATILE:
        return lora_config_write(cfg, 0xC2);
    case LORA_CFG_DEFAULT:
        lora_default_config(cfg);
        return lora_config_write(cfg, 0xC0);
    default:
        return false;
    }
}

bool LORA_Param(LORA_Param_t id, uint32_t *value, bool is_set)
{
    LORA_Config_t cfg;

    if(value == NULL)
        return false;

    if(is_set)
    {
        if(!LORA_Config(LORA_CFG_READ, &cfg))
            lora_default_config(&cfg);

        switch(id)
        {
        case LORA_PARAM_ADDR:
            cfg.addr_h = (uint8_t)((*value >> 8) & 0xFFU);
            cfg.addr_l = (uint8_t)(*value & 0xFFU);
            break;
        case LORA_PARAM_CHANNEL:
            if(*value > LORA_CHAN_MAX)
                return false;
            cfg.chan = (uint8_t)*value;
            break;
        case LORA_PARAM_AIR_RATE:
            if(*value > LORA_AIR_RATE_19K2BPS)
                return false;
            cfg.sped = (uint8_t)((cfg.sped & 0xF8U) | ((uint8_t)*value & 0x07U));
            break;
        case LORA_PARAM_TX_POWER:
            if(*value > LORA_TX_POWER_10DBM)
                return false;
            cfg.option = (uint8_t)((cfg.option & 0xFCU) | ((uint8_t)*value & 0x03U));
            break;
        default:
            return false;
        }
        return LORA_Config(LORA_CFG_WRITE, &cfg);
    }

    if(!LORA_Config(LORA_CFG_READ, &cfg))
        lora_default_config(&cfg);

    switch(id)
    {
    case LORA_PARAM_ADDR:
        *value = ((uint32_t)cfg.addr_h << 8) | cfg.addr_l;
        break;
    case LORA_PARAM_CHANNEL:
        *value = cfg.chan;
        break;
    case LORA_PARAM_AIR_RATE:
        *value = cfg.sped & 0x07U;
        break;
    case LORA_PARAM_TX_POWER:
        *value = cfg.option & 0x03U;
        break;
    default:
        return false;
    }
    return true;
}

bool LORA_ConfigPrint(void)
{
    LORA_Config_t cfg;
    uint16_t addr;
    uint8_t air_idx;
    uint8_t pwr_idx;
    static const char *air_rate_str[] = {
        "300bps", "1.2kbps", "2.4kbps", "4.8kbps", "9.6kbps", "19.2kbps"
    };
    static const char *tx_power_str[] = {
        "20dBm", "17dBm", "14dBm", "10dBm"
    };

    if(!LORA_Config(LORA_CFG_READ, &cfg))
    {
        printf("[LORA] 模块配置读取失败（未打印代码默认值）\r\n");
        return false;
    }

    addr = (uint16_t)(((uint16_t)cfg.addr_h << 8) | cfg.addr_l);
    air_idx = cfg.sped & 0x07U;
    pwr_idx = cfg.option & 0x03U;

    // printf("[LORA] addr=%u chan=%u(%luMHz) air=%s pwr=%s\r\n",
    //         (unsigned)addr,
    //         (unsigned)cfg.chan,
    //         (unsigned long)LORA_ChanToMHz(cfg.chan),
    //         (air_idx <= LORA_AIR_RATE_19K2BPS) ? air_rate_str[air_idx] : "?",
    //         (pwr_idx <= LORA_TX_POWER_10DBM) ? tx_power_str[pwr_idx] : "?");
    printf("LORA:addr -> %#02x ch -> %d(%dMHz)\r\nAIR:air -> %s\r\nPWR:power -> %s\r\n",
        addr,
        cfg.chan,
        LORA_ChanToMHz(cfg.chan),
        (air_idx <= LORA_AIR_RATE_19K2BPS) ? air_rate_str[air_idx] : "?",
        (pwr_idx <= LORA_TX_POWER_10DBM) ? tx_power_str[pwr_idx] : "?");
    return true;
}

bool LORA_SyncFromFlash(void)
{
    LORA_Config_t cfg;
    uint32_t cur_addr;
    bool need_write = false;

    if(!LORA_Config(LORA_CFG_READ, &cfg))
    {
        printf("[LORA] 同步失败: 读配置失败\r\n");
        return false;
    }

    cur_addr = ((uint32_t)cfg.addr_h << 8) | (uint32_t)cfg.addr_l;

    if(cur_addr != (uint32_t)sys_cfg.dev_addr)
    {
        cfg.addr_h = (uint8_t)(((uint32_t)sys_cfg.dev_addr >> 8) & 0xFFU);
        cfg.addr_l = (uint8_t)(sys_cfg.dev_addr & 0xFFU);
        need_write = true;
        printf("[LORA] 地址: %lu -> %u\r\n",
               (unsigned long)cur_addr, (unsigned)sys_cfg.dev_addr);
    }

    if(cfg.chan != LORA_DEFAULT_CHAN)
    {
        uint8_t old_chan = cfg.chan;

        cfg.chan = LORA_DEFAULT_CHAN;
        need_write = true;
        printf("[LORA] 信道: %u -> %u\r\n",
               (unsigned)old_chan, (unsigned)LORA_DEFAULT_CHAN);
    }

    if(need_write)
    {
        if(!LORA_Config(LORA_CFG_WRITE, &cfg))
        {
            printf("[LORA] 同步失败: 写配置失败\r\n");
            return false;
        }
        if(!LORA_Config(LORA_CFG_READ, &cfg))
        {
            printf("[LORA] 同步失败: 写入后读回失败\r\n");
            return false;
        }
    }

    printf("[LORA] 就绪 addr=%u chan=%u (%lu MHz)\r\n",
           (unsigned)sys_cfg.dev_addr,
           (unsigned)cfg.chan,
           (unsigned long)LORA_ChanToMHz(cfg.chan));
    return true;
}

static void lora_uart_mutex_init(void)
{
    if(s_lora_uart_mtx == NULL)
        s_lora_uart_mtx = xSemaphoreCreateMutex();
}

static bool lora_uart_transmit_raw(uint8_t *pdata, uint16_t len)
{
    if(pdata == NULL || len == 0U)
        return false;

    return (HAL_UART_Transmit(&huart5, pdata, len, 200) == HAL_OK);
}

static bool lora_cfg_session_begin(void)
{
    lora_uart_mutex_init();
    if(s_lora_uart_mtx == NULL)
        return false;

    if(xSemaphoreTake(s_lora_uart_mtx, pdMS_TO_TICKS(5000)) != pdTRUE)
        return false;

    s_lora_cfg_internal = 1U;
    s_lora_cfg_active = 1U;
#if LORA_UART_RX_IRQ
    lora_uart_rx_irq_disable();
#endif
    lora_uart_rx_flush();
    return true;
}

static void lora_cfg_session_end(void)
{
    lora_uart_rx_flush();
    s_lora_cfg_active = 0U;
    s_lora_cfg_internal = 0U;
#if LORA_UART_RX_IRQ
    lora_uart_rx_irq_enable();
#endif
    if(s_lora_uart_mtx != NULL)
        (void)xSemaphoreGive(s_lora_uart_mtx);
}

#if LORA_UART_RX_IRQ
static void lora_uart_rx_ring_reset(void)
{
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();
    s_lora_rx_head = 0U;
    s_lora_rx_tail = 0U;
    __set_PRIMASK(primask);
}

static bool lora_uart_rx_ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((s_lora_rx_head + 1U) % LORA_UART_RX_RING_SIZE);

    if(next == s_lora_rx_tail)
    {
        s_lora_rx_drop++;
        return false;
    }

    s_lora_rx_ring[s_lora_rx_head] = b;
    s_lora_rx_head = next;
    return true;
}

static bool lora_uart_rx_ring_pop(uint8_t *b)
{
    if(b == NULL || s_lora_rx_head == s_lora_rx_tail)
        return false;

    *b = s_lora_rx_ring[s_lora_rx_tail];
    s_lora_rx_tail = (uint16_t)((s_lora_rx_tail + 1U) % LORA_UART_RX_RING_SIZE);
    return true;
}

static void lora_uart_hw_drain(void)
{
    while(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
        (void)UART5->RDR;
    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart5);
}

static bool lora_uart_hw_poll_byte(uint8_t *b, bool *from_ore)
{
    if(b == NULL)
        return false;

    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        if(from_ore != NULL)
            *from_ore = false;
        return true;
    }

    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        __HAL_UART_CLEAR_OREFLAG(&huart5);
        if(from_ore != NULL)
            *from_ore = true;
        return true;
    }

    return false;
}

static void lora_uart_rx_irq_enable(void)
{
    lora_uart_hw_drain();
    __HAL_UART_CLEAR_FLAG(&huart5, UART_FLAG_RXNE);
    HAL_NVIC_SetPriority(UART5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
    __HAL_UART_ENABLE_IT(&huart5, UART_IT_RXNE);
    s_lora_rx_irq_en = 1U;
}

static void lora_uart_rx_irq_disable(void)
{
    s_lora_rx_irq_en = 0U;
    __HAL_UART_DISABLE_IT(&huart5, UART_IT_RXNE);
}

void LORA_UART_IRQHandler(void)
{
    if(!s_lora_rx_irq_en)
        return;

    while(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
        (void)lora_uart_rx_ring_push((uint8_t)(UART5->RDR & 0xFFU));

    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
    {
        (void)lora_uart_rx_ring_push((uint8_t)(UART5->RDR & 0xFFU));
        __HAL_UART_CLEAR_OREFLAG(&huart5);
    }
}
#else
void LORA_UART_IRQHandler(void)
{
}
#endif /* LORA_UART_RX_IRQ */

/*===========================================================================
 * 内部实现
 *===========================================================================*/

#if LORA_DEBUG
static void lora_dbg_hex(const char *tag, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if(tag != NULL)
        printf("[LORA_DBG] %s (%u):", tag, (unsigned)len);
    else
        printf("[LORA_DBG] HEX (%u):", (unsigned)len);

    if(data == NULL || len == 0U)
    {
        printf(" (空)\r\n");
        return;
    }

    for(i = 0; i < len; i++)
        printf(" %02X", data[i]);
    printf("\r\n");
}

static void lora_dbg_pins(void)
{
    printf("[LORA_DBG] 引脚 M0=%u M1=%u AUX=%u\r\n",
           (unsigned)HAL_GPIO_ReadPin(LORA_M0_GPIO_Port, LORA_M0_Pin),
           (unsigned)HAL_GPIO_ReadPin(LORA_M1_GPIO_Port, LORA_M1_Pin),
           (unsigned)HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin));
}

static void lora_dbg_uart_flags(void)
{
    printf("[LORA_DBG] UART5 RXNE=%u ORE=%u gState=%u RxState=%u\r\n",
           (unsigned)__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE),
           (unsigned)__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE),
           (unsigned)huart5.gState,
           (unsigned)huart5.RxState);
}
#endif

static void lora_delay_ms(uint32_t ms)
{
    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskDelay(pdMS_TO_TICKS(ms));
    else
        HAL_Delay(ms);
}

static bool lora_wait_aux_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET)
    {
        if((HAL_GetTick() - start) >= timeout_ms)
        {
            LORA_DBG("AUX 等待就绪超时 (%lu ms)\r\n", (unsigned long)timeout_ms);
#if LORA_DEBUG
            lora_dbg_pins();
#endif
            return false;
        }
        lora_delay_ms(1);
    }
    lora_delay_ms(2);
    LORA_DBG("AUX 就绪 (耗时 %lu ms)\r\n", (unsigned long)(HAL_GetTick() - start));
    return true;
}

/*
 * 模式切换后等待模块稳定：切换后 AUX 常会先拉低再拉高，不能见到“仍高”就立刻发令。
 */
static bool lora_wait_after_mode_change(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    bool saw_low = false;

    lora_delay_ms(5);

    while((HAL_GetTick() - start) < timeout_ms)
    {
        if(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET)
            saw_low = true;
        else if(saw_low)
        {
            lora_delay_ms(2);
            LORA_DBG("模式切换稳定 (AUX 低->高, %lu ms)\r\n",
                     (unsigned long)(HAL_GetTick() - start));
            return true;
        }
        taskYIELD();
    }

    if(HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET)
    {
        LORA_DBG("模式切换超时: AUX 仍为低\r\n");
#if LORA_DEBUG
        lora_dbg_pins();
#endif
        return false;
    }

    lora_delay_ms(2);
    LORA_DBG("模式切换稳定 (AUX 一直高, %lu ms)\r\n", (unsigned long)(HAL_GetTick() - start));
    return true;
}

static bool lora_uart_poll_byte(uint8_t *b, bool *from_ore)
{
#if LORA_UART_RX_IRQ
    if(s_lora_rx_irq_en && !s_lora_cfg_internal)
    {
        if(lora_uart_rx_ring_pop(b))
        {
            if(from_ore != NULL)
                *from_ore = false;
            return true;
        }
        return false;
    }
    return lora_uart_hw_poll_byte(b, from_ore);
#else
    if(b == NULL)
        return false;

    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        if(from_ore != NULL)
            *from_ore = false;
        return true;
    }

    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        __HAL_UART_CLEAR_OREFLAG(&huart5);
        if(from_ore != NULL)
            *from_ore = true;
        return true;
    }

    return false;
#endif
}

static void lora_uart_push_cfg_byte(uint8_t *buf, uint16_t *got, uint16_t cap, uint8_t b,
                                    uint16_t *discard)
{
    if(buf == NULL || got == NULL || *got >= cap)
        return;

    if(*got == 0U)
    {
        if(!lora_is_cfg_rsp_head(b))
        {
#if LORA_DEBUG
            if(discard != NULL)
            {
                (*discard)++;
                if(*discard <= 3U)
                    LORA_DBG("丢弃非帧头字节: %02X\r\n", b);
                else if(*discard == 4U)
                    LORA_DBG("后续杂字节省略打印...\r\n");
            }
#endif
            return;
        }
    }

    buf[(*got)++] = b;
}

static void lora_uart_state_reset(void)
{
    lora_uart_rx_flush();
    huart5.gState = HAL_UART_STATE_READY;
    huart5.RxState = HAL_UART_STATE_READY;
}

static void lora_uart_rx_flush(void)
{
#if LORA_UART_RX_IRQ
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();
    s_lora_rx_head = 0U;
    s_lora_rx_tail = 0U;
    __set_PRIMASK(primask);
    lora_uart_hw_drain();
#else
    while(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
        (void)UART5->RDR;
    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart5);
#endif
}

static void lora_uart_drain_rx(uint8_t *buf, uint16_t *got, uint16_t cap)
{
    uint8_t b;

    while(lora_uart_poll_byte(&b, NULL))
    {
        if(buf != NULL && got != NULL && *got < cap)
            buf[(*got)++] = b;
    }
}

static bool lora_is_cfg_rsp_head(uint8_t b)
{
    return (b == 0xC0) || (b == 0xC2);
}

/*
 * 读配置专用收帧：发 C1 后立即紧轮询收满 6 字节；挂起调度器避免高优先级任务抢占丢字节。
 */
static uint16_t lora_uart_read_cfg_rsp(uint8_t *buf, uint16_t cap, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint16_t got = 0;
    uint8_t b;
    bool from_ore;
    bool suspended = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);

    if(buf == NULL || cap < LORA_CFG_RSP_LEN)
        return 0;

    if(suspended)
        vTaskSuspendAll();

    while(got < LORA_CFG_RSP_LEN && (HAL_GetTick() - start) < timeout_ms)
    {
        if(lora_uart_poll_byte(&b, &from_ore))
            lora_uart_push_cfg_byte(buf, &got, cap, b, NULL);
    }

    if(got < LORA_CFG_RSP_LEN)
    {
        while(got < LORA_CFG_RSP_LEN && __HAL_UART_GET_FLAG(&huart5, UART_FLAG_RXNE))
        {
            b = (uint8_t)(UART5->RDR & 0xFFU);
            lora_uart_push_cfg_byte(buf, &got, cap, b, NULL);
        }
    }

    if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart5);

    if(suspended)
        (void)xTaskResumeAll();

    LORA_DBG("收配置应答: %u 字节, 耗时 %lu ms\r\n",
             (unsigned)got, (unsigned long)(HAL_GetTick() - start));
#if LORA_DEBUG
    if(got > 0U)
        lora_dbg_hex("RX", buf, got);
#endif

    return got;
}

/*
 * 配置/透传轮询收帧：直接读 RDR，避免 HAL 单字节接收与固定 delay 导致 ORE 丢字节。
 * expect_len>0 时优先收满该长度；expect_len==0 且 timeout_ms==0 时仅排空当前 FIFO。
 */
static uint16_t lora_uart_read_frame(uint8_t *buf, uint16_t cap, uint16_t expect_len, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint32_t last_rx = start;
    uint16_t got = 0;

    if(buf == NULL || cap == 0U)
        return 0;

    if(expect_len == 0U && timeout_ms == 0U)
    {
        while(got < cap && lora_uart_poll_byte(&buf[got], NULL))
            got++;
#if LORA_UART_RX_IRQ
        if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
            __HAL_UART_CLEAR_OREFLAG(&huart5);
#else
        __HAL_UART_CLEAR_OREFLAG(&huart5);
#endif

        if(got > 0U)
        {
            printf("LORA RX:");
            for(uint16_t i = 0; i < got; i++)
            {
                printf("%c", buf[i]);
            }
            printf("\r\n");
        }
        return got;
    }

    while((HAL_GetTick() - start) < timeout_ms)
    {
        uint8_t b;

        if(lora_uart_poll_byte(&b, NULL))
        {
            if(got < cap)
                buf[got++] = b;
            last_rx = HAL_GetTick();

            if(expect_len > 0U && got >= expect_len)
                break;
            continue;
        }

        if(__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE))
            __HAL_UART_CLEAR_OREFLAG(&huart5);

        if(got > 0U && expect_len > 0U && got >= expect_len)
            break;

        if(got > 0U && expect_len == 0U &&
           (HAL_GetTick() - last_rx) >= LORA_UART_FRAME_GAP_MS)
            break;

        if(got > 0U &&
           HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) != GPIO_PIN_RESET &&
           (HAL_GetTick() - last_rx) >= 2U)
        {
            lora_uart_drain_rx(buf, &got, cap);
            break;
        }

        taskYIELD();
    }

    __HAL_UART_CLEAR_OREFLAG(&huart5);
    return got;
}

static bool lora_parse_config_frame(const uint8_t *frame, uint16_t len, LORA_Config_t *config)
{
    const uint8_t *p;

    if(config == NULL || frame == NULL)
        return false;

    /* 读配置标准应答：C0/C2 + ADDH + ADDL + SPED + CHAN + OPTION（共 6 字节） */
    if(len >= LORA_CFG_RSP_LEN && (frame[0] == 0xC0 || frame[0] == 0xC2))
        p = &frame[1];
    else if(len >= 5U && frame[0] != 0xC0 && frame[0] != 0xC1 && frame[0] != 0xC2)
        p = &frame[0];
    else
    {
        LORA_DBG("解析失败: len=%u head=%02X\r\n", (unsigned)len,
                 (len > 0U) ? frame[0] : 0U);
        return false;
    }

    config->addr_h = p[0];
    config->addr_l = p[1];
    config->sped   = p[2];
    config->chan   = p[3];
    config->option = p[4];
    return true;
}

static bool lora_config_read(LORA_Config_t *config)
{
    static const uint8_t read_cmd[] = {0xC1, 0xC1, 0xC1};
    uint8_t rx[8];
    uint16_t rx_len = 0;
    bool ok = false;
    uint8_t attempt;

    if(config == NULL)
        return false;

    if(!lora_cfg_session_begin())
        return false;

    lora_uart_state_reset();

    if(!lora_set_mode(true))
    {
        LORA_DBG("读配置失败: 无法进入配置模式\r\n");
        goto exit_read;
    }

    for(attempt = 0; attempt < 3 && !ok; attempt++)
    {
        LORA_DBG("读配置尝试 %u/3\r\n", (unsigned)(attempt + 1U));

        if(!lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS))
        {
            LORA_DBG("跳过本趟: AUX 未就绪\r\n");
            continue;
        }

        lora_uart_rx_flush();
#if LORA_DEBUG
        lora_dbg_hex("TX", read_cmd, (uint16_t)sizeof(read_cmd));
#endif
        if(!lora_uart_transmit_raw((uint8_t *)read_cmd, sizeof(read_cmd)))
        {
            LORA_DBG("发送 C1C1C1 失败\r\n");
            break;
        }

        rx_len = lora_uart_read_cfg_rsp(rx, sizeof(rx), LORA_CFG_RX_TIMEOUT_MS);
        if(rx_len >= LORA_CFG_RSP_LEN)
            ok = lora_parse_config_frame(rx, rx_len, config);
        else
            LORA_DBG("本趟长度不足: %u (需要 %u)\r\n",
                    (unsigned)rx_len, (unsigned)LORA_CFG_RSP_LEN);
    }

    if(!ok)
    {
        printf("[LORA] 读配置无有效应答 (rx=%u", (unsigned)rx_len);
        if(rx_len > 0)
        {
            uint8_t i;
            printf(", raw=");
            for(i = 0; i < rx_len && i < sizeof(rx); i++)
                printf(" %02X", rx[i]);
        }
        printf(")\r\n");
    }

    lora_set_mode(false);

exit_read:
    lora_cfg_session_end();
    return ok;
}

static bool lora_config_write(const LORA_Config_t *config, uint8_t head)
{
    uint8_t tx[6];
    bool ok = false;

    if(config == NULL)
        return false;

    if(!lora_cfg_session_begin())
        return false;

    lora_uart_state_reset();

    if(!lora_set_mode(true))
    {
        LORA_DBG("写配置失败: 无法进入配置模式\r\n");
        goto exit_write;
    }

    lora_uart_rx_flush();

    tx[0] = head;
    tx[1] = config->addr_h;
    tx[2] = config->addr_l;
    tx[3] = config->sped;
    tx[4] = config->chan;
    tx[5] = config->option;
#if LORA_DEBUG
    lora_dbg_hex("TX", tx, (uint16_t)sizeof(tx));
#endif
    if(!lora_uart_transmit_raw(tx, sizeof(tx)))
    {
        LORA_DBG("写配置发送失败\r\n");
        goto exit_write_mode;
    }

    ok = lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS);
    LORA_DBG("写配置 %s\r\n", ok ? "完成" : "AUX 超时");

exit_write_mode:
    lora_set_mode(false);

exit_write:
    lora_cfg_session_end();
    return ok;
}

static void lora_default_config(LORA_Config_t *config)
{
    if(config == NULL)
        return;

    /* 地址与 Flash 中 dev_addr 一致；信道/速率/功率恢复 E32 缺省 */
    config->addr_h = (uint8_t)(((uint32_t)sys_cfg.dev_addr >> 8) & 0xFFU);
    config->addr_l = (uint8_t)(sys_cfg.dev_addr & 0xFFU);
    config->sped   = LORA_DEFAULT_SPED;
    config->chan   = LORA_DEFAULT_CHAN;
    config->option = LORA_DEFAULT_OPTION;
}

static bool lora_set_mode(bool config_mode)
{
    /* 手册：仅在 AUX 空闲（高）时切换模式 */
    LORA_DBG("切换模式 -> %s\r\n", config_mode ? "配置(M0=1,M1=1)" : "正常(M0=0,M1=0)");

    if(!lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS))
    {
        LORA_DBG("模式切换取消: 切换前 AUX 未就绪\r\n");
        return false;
    }

    if(config_mode)
    {
        HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_RESET);
    }

    if(!lora_wait_after_mode_change(LORA_AUX_TIMEOUT_MS))
    {
        LORA_DBG("模式切换失败: 切换后未稳定\r\n");
#if LORA_DEBUG
        lora_dbg_pins();
#endif
        return false;
    }

#if LORA_DEBUG
    lora_dbg_pins();
#endif
    return true;
}

//static bool lora_sync_param(LORA_Param_t id, uint32_t target, const char *tag)
//{
//    uint32_t cur = 0;
//    uint8_t n;

//    for(n = 0; n < LORA_SYNC_MAX_RETRY; n++)
//    {
//        if(!LORA_Param(id, &cur, false))
//        {
//            printf("[LORA] %s 读失败 (%u/%u)\r\n",
//                   tag, (unsigned)(n + 1U), (unsigned)LORA_SYNC_MAX_RETRY);
//            continue;
//        }

//        if(cur == target)
//            return true;

//        printf("[LORA] %s: %lu -> %lu (%u/%u)\r\n",
//               tag, (unsigned long)cur, (unsigned long)target,
//               (unsigned)(n + 1U), (unsigned)LORA_SYNC_MAX_RETRY);
//        if(!LORA_Param(id, &target, true))
//            printf("[LORA] %s 写入失败 (%u/%u)\r\n",
//                   tag, (unsigned)(n + 1U), (unsigned)LORA_SYNC_MAX_RETRY);
//    }

//    if(LORA_Param(id, &cur, false) && cur == target)
//        return true;

//    printf("[LORA] %s 同步失败: 目标=%lu 当前=%lu\r\n",
//           tag, (unsigned long)target, (unsigned long)cur);
//    return false;
//}
