#include "lora.h"

#include "fsy_dispatch.h"
#include "fsy_frame.h"
#include "main.h"
#include "ota.h"
#include "uart_diag.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef s_huart5;
static volatile uint8_t s_lora_ready;
static volatile uint8_t s_lora_sw_enable = 1U;

#define LORA_POLL_MAX_LOOPS     32U

static uint8_t  s_lora_asm_buf[FSY_FRAME_MAX_LEN];
static uint16_t s_lora_asm_len;
static uint32_t s_lora_asm_last_tick;

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
static bool lora_boot_apply_config(void);
static bool lora_boot_print_config(void);
static bool lora_config_read(LORA_Config_t *config);
static bool lora_config_write(const LORA_Config_t *config, uint8_t head);
static bool lora_at_default(void);
//static bool lora_sync_param(LORA_Param_t id, uint32_t target, const char *tag);
static void lora_uart_mutex_init(void);
static bool lora_uart_transmit_raw(uint8_t *pdata, uint16_t len);
static bool lora_cfg_session_begin(void);
static void lora_cfg_session_end(void);
static bool lora_uart_reinit_baud(uint32_t baud);
#if LORA_UART_RX_IRQ
static void lora_uart_rx_ring_reset(void);
static bool lora_uart_rx_ring_push(uint8_t b);
static bool lora_uart_rx_ring_pop(uint8_t *b);
static void lora_uart_hw_drain(void);
static bool lora_uart_hw_poll_byte(uint8_t *b, bool *from_ore);
static void lora_uart_rx_irq_enable(void);
static void lora_uart_rx_irq_disable(void);
#endif

static bool lora_hw_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin, GPIO_PIN_RESET);

    gpio.Pin = LORA_M0_Pin | LORA_M1_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LORA_M0_GPIO_Port, &gpio);

    gpio.Pin = LORA_AUX_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LORA_AUX_GPIO_Port, &gpio);

    return true;
}

static bool lora_hw_uart5_init(void)
{
    RCC_PeriphCLKInitTypeDef clk = {0};
    GPIO_InitTypeDef gpio = {0};

    if (s_huart5.Instance != NULL) {
        return true;
    }

    clk.PeriphClockSelection = RCC_PERIPHCLK_UART5;
    clk.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) {
        return false;
    }

    __HAL_RCC_UART5_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio.Pin = LORA_TX_Pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(LORA_TX_GPIO_Port, &gpio);

    gpio.Pin = LORA_RX_Pin;
    HAL_GPIO_Init(LORA_RX_GPIO_Port, &gpio);

    s_huart5.Instance = UART5;
    s_huart5.Init.BaudRate = 9600;
    s_huart5.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart5.Init.StopBits = UART_STOPBITS_1;
    s_huart5.Init.Parity = UART_PARITY_NONE;
    s_huart5.Init.Mode = UART_MODE_TX_RX;
    s_huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart5.Init.OverSampling = UART_OVERSAMPLING_16;
    s_huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_huart5.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    s_huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_huart5) != HAL_OK) {
        return false;
    }

    HAL_NVIC_SetPriority(UART5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
    return true;
}

static bool lora_hw_init(void)
{
    if (!lora_hw_gpio_init()) {
        return false;
    }
    return lora_hw_uart5_init();
}

/*===========================================================================
 * 底层驱动接口（UART5）：Init / Transmit / Receive
 *===========================================================================*/

bool LORA_Init(void)
{
    bool ok;

    if (s_lora_sw_enable == 0U) {
        return false;
    }

    s_lora_ready = 0U;
    if (!lora_hw_init()) {
        UartDiag_Write("[LORA] UART5/GPIO init fail\r\n");
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
    if (ok) {
        ok = lora_boot_apply_config();
        if (ok) {
#if LORA_UART_RX_IRQ
            lora_uart_rx_ring_reset();
            lora_uart_rx_irq_enable();
#endif
            s_lora_ready = 1U;
        } else {
            UartDiag_Write("[LORA] boot apply fail\r\n");
        }
    } else {
        UartDiag_Write("[LORA] init fail (AUX timeout?)\r\n");
    }
    return ok;
}

bool LORA_Transmit(uint8_t *pdata, uint16_t len)
{
    bool ok;

    if (s_lora_sw_enable == 0U || s_lora_ready == 0U) {
        return false;
    }

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

    if(ok) {
        printf("[LORA] TX %u bytes\r\n", (unsigned)len);
    }

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
    if ((unsigned)op < 4U) {
        static const char *op_name[] = {"READ", "WRITE", "WRITE_VOL", "DEFAULT"};
        LORA_DBG("Config 操作: %s\r\n", op_name[op]);
    }

    switch(op)
    {
    case LORA_CFG_READ:
        return lora_config_read(cfg);
    case LORA_CFG_WRITE:
        return lora_config_write(cfg, 0xC0);
    case LORA_CFG_WRITE_VOLATILE:
        return lora_config_write(cfg, 0xC2);
    case LORA_CFG_DEFAULT:
        return lora_at_default();
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

static bool lora_boot_print_config(void)
{
    LORA_Config_t cfg;
    char msg[160];
    uint16_t addr;
    uint8_t air_idx;
    uint8_t pwr_idx;
    static const char *air_rate_str[] = {
        "300bps", "1.2kbps", "2.4kbps", "4.8kbps", "9.6kbps", "19.2kbps"
    };
    static const char *tx_power_str[] = {
        "20dBm", "17dBm", "14dBm", "10dBm"
    };

    if (!LORA_Config(LORA_CFG_READ, &cfg)) {
        UartDiag_Write("[LORA] ready 115200 mode0 (config read fail)\r\n");
        return false;
    }

    addr = (uint16_t)(((uint16_t)cfg.addr_h << 8) | cfg.addr_l);
    air_idx = cfg.sped & 0x07U;
    pwr_idx = cfg.option & 0x03U;

    (void)snprintf(msg, sizeof(msg),
                   "[LORA] ready 115200 mode0 addr=%u chan=%u(%luMHz) "
                   "sped=0x%02X opt=0x%02X air=%s pwr=%s\r\n",
                   (unsigned)addr,
                   (unsigned)cfg.chan,
                   (unsigned long)LORA_ChanToMHz(cfg.chan),
                   (unsigned)cfg.sped,
                   (unsigned)cfg.option,
                   (air_idx <= LORA_AIR_RATE_19K2BPS) ? air_rate_str[air_idx] : "?",
                   (pwr_idx <= LORA_TX_POWER_10DBM) ? tx_power_str[pwr_idx] : "?");
    UartDiag_Write(msg);
    return true;
}

static bool lora_cfg_matches_target(const LORA_Config_t *cfg)
{
    LORA_Config_t target;

    if (cfg == NULL) {
        return false;
    }

    lora_default_config(&target);
    return (cfg->addr_h == target.addr_h) &&
           (cfg->addr_l == target.addr_l) &&
           (cfg->sped == target.sped) &&
           (cfg->chan == target.chan) &&
           (cfg->option == target.option);
}

static bool lora_uart_reinit_baud(uint32_t baud)
{
#if LORA_UART_RX_IRQ
    lora_uart_rx_irq_disable();
#endif

    if (HAL_UART_DeInit(&s_huart5) != HAL_OK) {
        return false;
    }

    s_huart5.Init.BaudRate = baud;
    if (HAL_UART_Init(&s_huart5) != HAL_OK) {
        return false;
    }

    HAL_NVIC_SetPriority(UART5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
    return true;
}

static bool lora_boot_apply_config(void)
{
    LORA_Config_t cfg;
    LORA_Config_t target;
    bool ok;

    lora_default_config(&target);

    if (!LORA_Config(LORA_CFG_READ, &cfg)) {
        ok = LORA_Config(LORA_CFG_WRITE, &target);
        if (!ok) {
            UartDiag_Write("[LORA] boot write fail\r\n");
            return false;
        }
        ok = LORA_Config(LORA_CFG_READ, &cfg);
    } else if (!lora_cfg_matches_target(&cfg)) {
        ok = LORA_Config(LORA_CFG_WRITE, &target);
        if (!ok) {
            UartDiag_Write("[LORA] boot write fail\r\n");
            return false;
        }
        ok = LORA_Config(LORA_CFG_READ, &cfg);
    } else {
        ok = true;
    }

    if (!ok) {
        UartDiag_Write("[LORA] boot read fail\r\n");
        return false;
    }

    (void)lora_boot_print_config();

    if (!lora_uart_reinit_baud(LORA_USART_BAUD_RUN)) {
        UartDiag_Write("[LORA] UART5 115200 fail\r\n");
        return false;
    }

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

    return (HAL_UART_Transmit(&s_huart5, pdata, len, 200) == HAL_OK);
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
    while(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE))
        (void)UART5->RDR;
    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);
}

static bool lora_uart_hw_poll_byte(uint8_t *b, bool *from_ore)
{
    if(b == NULL)
        return false;

    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        if(from_ore != NULL)
            *from_ore = false;
        return true;
    }

    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);
        if(from_ore != NULL)
            *from_ore = true;
        return true;
    }

    return false;
}

static void lora_uart_rx_irq_enable(void)
{
    lora_uart_hw_drain();
    __HAL_UART_CLEAR_FLAG(&s_huart5, UART_FLAG_RXNE);
    HAL_NVIC_SetPriority(UART5_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
    __HAL_UART_ENABLE_IT(&s_huart5, UART_IT_RXNE);
    s_lora_rx_irq_en = 1U;
}

static void lora_uart_rx_irq_disable(void)
{
    s_lora_rx_irq_en = 0U;
    __HAL_UART_DISABLE_IT(&s_huart5, UART_IT_RXNE);
}

void LORA_UART_IRQHandler(void)
{
    if(!s_lora_rx_irq_en)
        return;

    while(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE))
        (void)lora_uart_rx_ring_push((uint8_t)(UART5->RDR & 0xFFU));

    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
    {
        (void)lora_uart_rx_ring_push((uint8_t)(UART5->RDR & 0xFFU));
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);
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
           (unsigned)__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE),
           (unsigned)__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE),
           (unsigned)s_huart5.gState,
           (unsigned)s_huart5.RxState);
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

    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        if(from_ore != NULL)
            *from_ore = false;
        return true;
    }

    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
    {
        *b = (uint8_t)(UART5->RDR & 0xFFU);
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);
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
    s_huart5.gState = HAL_UART_STATE_READY;
    s_huart5.RxState = HAL_UART_STATE_READY;
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
    while(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE))
        (void)UART5->RDR;
    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);
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
        while(got < LORA_CFG_RSP_LEN && __HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_RXNE))
        {
            b = (uint8_t)(UART5->RDR & 0xFFU);
            lora_uart_push_cfg_byte(buf, &got, cap, b, NULL);
        }
    }

    if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);

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
        if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
            __HAL_UART_CLEAR_OREFLAG(&s_huart5);
#else
        __HAL_UART_CLEAR_OREFLAG(&s_huart5);
#endif

        if(got > 0U)
        {
            LORA_DBG("非阻塞收 %u 字节\r\n", (unsigned)got);
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

        if(__HAL_UART_GET_FLAG(&s_huart5, UART_FLAG_ORE))
            __HAL_UART_CLEAR_OREFLAG(&s_huart5);

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

    __HAL_UART_CLEAR_OREFLAG(&s_huart5);
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

    config->addr_h = LORA_DEFAULT_ADDR_H;
    config->addr_l = LORA_DEFAULT_ADDR_L;
    config->sped   = LORA_DEFAULT_SPED;
    config->chan   = LORA_DEFAULT_CHAN;
    config->option = LORA_DEFAULT_OPTION;
}

static bool lora_at_rx_has_ok(const uint8_t *rx, uint16_t len)
{
    uint16_t i;

    if (rx == NULL || len < 2U) {
        return false;
    }

    for (i = 0U; i + 1U < len; i++) {
        if (rx[i] == (uint8_t)'O' && rx[i + 1U] == (uint8_t)'K') {
            return true;
        }
    }
    return false;
}

static bool lora_at_default(void)
{
    static const char cmd[] = "AT+DEFAULT\r\n";
    uint8_t rx[32];
    uint16_t rx_len = 0U;
    bool ok = false;
    bool aux_ok;

    if (!lora_cfg_session_begin()) {
        return false;
    }

    lora_uart_state_reset();

    if (!lora_set_mode(true)) {
        goto exit_at;
    }

    lora_uart_rx_flush();

    if (!lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS)) {
        goto exit_at;
    }

    if (!lora_uart_transmit_raw((uint8_t *)cmd, (uint16_t)(sizeof(cmd) - 1U))) {
        goto exit_at;
    }

    {
        uint32_t start = HAL_GetTick();
        while (rx_len < sizeof(rx) && (HAL_GetTick() - start) < 800U) {
            uint8_t b;
            bool from_ore;

            if (lora_uart_poll_byte(&b, &from_ore)) {
                rx[rx_len++] = b;
            }
        }
    }

    aux_ok = lora_wait_aux_ready(LORA_AUX_TIMEOUT_MS);
    ok = lora_at_rx_has_ok(rx, rx_len) || aux_ok;

exit_at:
    (void)lora_set_mode(false);
    lora_cfg_session_end();
    return ok;
}

bool LORA_AtDefault(void)
{
    bool ok = lora_at_default();
    if (ok) {
        UartDiag_Write("[LORA] AT+DEFAULT ok\r\n");
    } else {
        UartDiag_Write("[LORA] AT+DEFAULT fail\r\n");
    }
    return ok;
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

bool LORA_IsReady(void)
{
    return (s_lora_sw_enable != 0U) && (s_lora_ready != 0U);
}

bool LORA_IsEnabled(void)
{
    return (s_lora_sw_enable != 0U);
}

static void lora_clear_runtime(void)
{
    s_lora_asm_len = 0U;
    s_lora_asm_last_tick = 0U;
#if LORA_UART_RX_IRQ
    lora_uart_rx_ring_reset();
#endif
}

bool LORA_SetEnabled(bool enable)
{
    if (enable) {
        s_lora_sw_enable = 1U;
        if (s_lora_ready == 0U) {
            return LORA_Init();
        }
        return true;
    }

    s_lora_sw_enable = 0U;
    if (s_lora_cfg_active != 0U) {
        (void)lora_set_mode(false);
        s_lora_cfg_active = 0U;
        s_lora_cfg_internal = 0U;
    }
    lora_clear_runtime();
    s_lora_ready = 0U;
#if LORA_UART_RX_IRQ
    lora_uart_rx_irq_disable();
#endif
    return true;
}

#define LORA_LOG_RX_HEX_MAX  64U

static void lora_log_rx_data(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t show;

    if (data == NULL || len == 0U) {
        return;
    }

    show = len;
    if (show > LORA_LOG_RX_HEX_MAX) {
        show = LORA_LOG_RX_HEX_MAX;
    }

    printf("[LORA] RX %u bytes:", (unsigned)len);
    for (i = 0U; i < show; i++) {
        printf(" %02X", data[i]);
    }
    if (len > show) {
        printf(" ...");
    }
    printf("\r\n");
}

void LORA_Poll(void)
{
    uint8_t chunk[LORA_REC_LEN];
    uint16_t n;
    uint16_t loops = 0U;
    uint8_t my_addr = Fsy_Dispatch_GetDeviceAddr();

    if (s_lora_sw_enable == 0U) {
        return;
    }

    if ((s_lora_ready == 0U) || (s_lora_cfg_active != 0U)) {
        return;
    }

    n = LORA_Receive(chunk, 0, 0);
    if (n > 0U) {
        lora_log_rx_data(chunk, n);
        if ((s_lora_asm_len + n) > (uint16_t)sizeof(s_lora_asm_buf)) {
            s_lora_asm_len = 0U;
        }
        memcpy(&s_lora_asm_buf[s_lora_asm_len], chunk, n);
        s_lora_asm_len = (uint16_t)(s_lora_asm_len + n);
        s_lora_asm_last_tick = HAL_GetTick();
    }

    for (;;) {
        uint16_t frame_len;
        uint8_t resp[FSY_FRAME_MAX_LEN];
        int resp_len;

        if (s_lora_asm_len == 0U) {
            break;
        }

        frame_len = Fsy_Frame_RtuAssembleLen(s_lora_asm_buf, s_lora_asm_len);
        if (frame_len == FSY_FRAME_LEN_NEED_MORE) {
            if ((HAL_GetTick() - s_lora_asm_last_tick) > 200U) {
                s_lora_asm_len = 0U;
            }
            break;
        }
        if (frame_len == 0U) {
            memmove(s_lora_asm_buf, &s_lora_asm_buf[1], s_lora_asm_len - 1U);
            s_lora_asm_len--;
            continue;
        }
        if (frame_len > (uint16_t)sizeof(s_lora_asm_buf)) {
            memmove(s_lora_asm_buf, &s_lora_asm_buf[1], s_lora_asm_len - 1U);
            s_lora_asm_len--;
            continue;
        }
        if (s_lora_asm_len < frame_len) {
            break;
        }

        if (!Fsy_Frame_CrcOk(s_lora_asm_buf, frame_len) ||
            !Fsy_Frame_FormatOk(s_lora_asm_buf, frame_len)) {
            memmove(s_lora_asm_buf, &s_lora_asm_buf[1], s_lora_asm_len - 1U);
            s_lora_asm_len--;
            continue;
        }

        if (s_lora_asm_buf[0] != my_addr) {
            if (s_lora_asm_len > frame_len) {
                memmove(s_lora_asm_buf, &s_lora_asm_buf[frame_len],
                        s_lora_asm_len - frame_len);
                s_lora_asm_len = (uint16_t)(s_lora_asm_len - frame_len);
            } else {
                s_lora_asm_len = 0U;
            }
            continue;
        }

        resp_len = Fsy_Dispatch_Request(s_lora_asm_buf, frame_len, resp, sizeof(resp));
        if (s_lora_asm_len > frame_len) {
            memmove(s_lora_asm_buf, &s_lora_asm_buf[frame_len], s_lora_asm_len - frame_len);
            s_lora_asm_len = (uint16_t)(s_lora_asm_len - frame_len);
        } else {
            s_lora_asm_len = 0U;
        }

        if (resp_len > 0) {
            (void)LORA_Transmit(resp, (uint16_t)resp_len);
        }
        (void)OTA_CommitPending();

        loops++;
        if (loops >= LORA_POLL_MAX_LOOPS) {
            break;
        }
    }
}
