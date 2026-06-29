#include "ws2812b.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"

#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_dma.h"
#include "cmsis_os.h"
#include "uart_diag.h"

extern DMA_HandleTypeDef hdma_tim4_ch1;

uint32_t ws2812_code0;
uint32_t ws2812_code1;

uint32_t State_Pixel_Buf[WS2812_STATE_BUF_ROWS][24];
static uint32_t rgb_color_upload[TOTAL_LED_COUNT];

static ws2812_pattern_t s_pattern = WS2812_PATTERN_OFF;
static uint8_t s_chase_head;
static uint8_t s_step_acc;

#define CHASE_FAST_TICKS   2U
#define CHASE_SLOW_TICKS   8U
#define CHASE_TRAIL_FAST   3U
#define CHASE_TRAIL_SLOW   2U

static uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint16_t)g << 8) | b;
}

static void fill_all(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t i;
    uint32_t c = pack_rgb(r, g, b);

    for (i = 0U; i < TOTAL_LED_COUNT; i++) {
        rgb_color_upload[i] = c;
    }
}

static void fill_chase(uint8_t r, uint8_t g, uint8_t b, uint8_t head, uint8_t trail_len)
{
    uint8_t i;
    uint8_t t;

    for (i = 0U; i < TOTAL_LED_COUNT; i++) {
        rgb_color_upload[i] = 0U;
    }

    for (t = 0U; t < trail_len; t++) {
        uint8_t idx = (uint8_t)((head + TOTAL_LED_COUNT - t) % TOTAL_LED_COUNT);
        uint8_t scale = (uint8_t)(255U - (uint32_t)t * 255U / trail_len);

        rgb_color_upload[idx] = pack_rgb((uint8_t)((uint16_t)r * scale / 255U),
                                         (uint8_t)((uint16_t)g * scale / 255U),
                                         (uint8_t)((uint16_t)b * scale / 255U));
    }
}

static void pattern_step(void)
{
    uint8_t ticks;
    uint8_t trail = CHASE_TRAIL_SLOW;

    switch (s_pattern) {
    case WS2812_PATTERN_OFF:
        fill_all(0U, 0U, 0U);
        return;
    case WS2812_PATTERN_RED_SOLID:
        fill_all(255U, 0U, 0U);
        return;
    case WS2812_PATTERN_RED_FAST:
        ticks = CHASE_FAST_TICKS;
        trail = CHASE_TRAIL_FAST;
        fill_chase(255U, 0U, 0U, s_chase_head, trail);
        break;
    case WS2812_PATTERN_RED_SLOW:
        ticks = CHASE_SLOW_TICKS;
        trail = CHASE_TRAIL_SLOW;
        fill_chase(255U, 0U, 0U, s_chase_head, trail);
        break;
    case WS2812_PATTERN_WHITE_SLOW:
    default:
        ticks = CHASE_SLOW_TICKS;
        trail = CHASE_TRAIL_SLOW;
        fill_chase(220U, 220U, 220U, s_chase_head, trail);
        break;
    }

    s_step_acc++;
    if (s_step_acc >= ticks) {
        s_step_acc = 0U;
        s_chase_head++;
        if (s_chase_head >= TOTAL_LED_COUNT) {
            s_chase_head = 0U;
        }
    }
}

void ws2812_set_pattern(ws2812_pattern_t pattern)
{
#if !NEIJI_WS2812_ENABLE
    (void)pattern;
    return;
#else
    if (pattern != s_pattern) {
        s_pattern = pattern;
        s_chase_head = 0U;
        s_step_acc = 0U;
    }
#endif
}

ws2812_pattern_t ws2812_get_pattern(void)
{
    return s_pattern;
}

void ws2812b_init(void)
{
    RCC_ClkInitTypeDef clkcfg = {0};
    uint32_t fl = 0U;
    uint64_t timhz;
    uint32_t arrp1;
    uint32_t c0;
    uint32_t c1;

    HAL_RCC_GetClockConfig(&clkcfg, &fl);
    timhz = (uint64_t)HAL_RCC_GetPCLK1Freq();
    if (clkcfg.APB1CLKDivider != RCC_HCLK_DIV1) {
        timhz *= 2ULL;
    }
    timhz /= (uint64_t)(htim4.Init.Prescaler + 1U);

    arrp1 = (uint32_t)((timhz * 1500ULL + 500000000ULL) / 1000000000ULL);
    if (arrp1 < 4U) {
        arrp1 = 4U;
    }
    c0 = (uint32_t)((timhz * 340ULL + 500000000ULL) / 1000000000ULL);
    c1 = (uint32_t)((timhz * 750ULL + 500000000ULL) / 1000000000ULL);
    if (c0 + 1U >= arrp1) {
        c0 = arrp1 - 2U;
    }
    if (c1 + 1U >= arrp1) {
        c1 = arrp1 - 2U;
    }

    ws2812_code0 = c0;
    ws2812_code1 = c1;
    __HAL_TIM_SET_AUTORELOAD(&htim4, arrp1 - 1U);
}

void rgb_led_flush(void)
{
    uint8_t i;
    RGB_Color_TypeDef led_colors[TOTAL_LED_COUNT];

    for (i = 0U; i < TOTAL_LED_COUNT; i++) {
        uint32_t color = rgb_color_upload[i];
        led_colors[i].G = (color >> 8) & 0xFFU;
        led_colors[i].R = (color >> 16) & 0xFFU;
        led_colors[i].B = color & 0xFFU;
    }

    for (i = 0U; i < TOTAL_LED_COUNT; i++) {
        uint8_t j;
        for (j = 0U; j < 8U; j++) {
            State_Pixel_Buf[i][j] =
                (((uint8_t)(led_colors[i].G * Breath_Bright_State) & (1U << (7U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
        }
        for (j = 8U; j < 16U; j++) {
            State_Pixel_Buf[i][j] =
                (((uint8_t)(led_colors[i].R * Breath_Bright_State) & (1U << (15U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
        }
        for (j = 16U; j < 24U; j++) {
            State_Pixel_Buf[i][j] =
                (((uint8_t)(led_colors[i].B * Breath_Bright_State) & (1U << (23U - j))) != 0U) ? ws2812_code1 : ws2812_code0;
        }
    }

    {
        uint16_t row;
        for (row = TOTAL_LED_COUNT; row < TOTAL_LED_COUNT + WS2812_RESET_ROWS; row++) {
            uint8_t j;
            for (j = 0U; j < 24U; j++) {
                State_Pixel_Buf[row][j] = 0U;
            }
        }
    }

    SCB_CleanDCache_by_Addr(&State_Pixel_Buf[0][0], WS2812_STATE_BUF_ROWS * 24);
    HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t *)State_Pixel_Buf,
                          (uint32_t)(TOTAL_LED_COUNT + WS2812_RESET_ROWS) * 24U);
}

static void ws2812_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)pvParameters;

    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        pattern_step();
        rgb_led_flush();
        vTaskDelayUntil(&last_wake, COMMON_DELAY);
    }
}

void ws2812b_TaskInit(void)
{
#if !NEIJI_WS2812_ENABLE
    UartDiag_Write("[WS2812] disabled (NEIJI_WS2812_ENABLE=0)\r\n");
    return;
#else
    static osThreadId_t handle;
    static const osThreadAttr_t attr = {
        .name = "ws2812Task",
        .stack_size = 512 * 4,
        .priority = osPriorityNormal,
    };

    ws2812b_init();
    ws2812_set_pattern(WS2812_PATTERN_WHITE_SLOW);
    handle = osThreadNew(ws2812_task, NULL, &attr);
    (void)handle;
#endif
}
