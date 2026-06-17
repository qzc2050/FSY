#ifndef _WS2812B_H_
#define _WS2812B_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "main.h"



#define COLOR_COUNT         21
#define TOTAL_LED_COUNT     8
#define COMMON_DELAY        20     // LED REFRESH TIME



/*
 * WS2812B-2020 目标时序（单 PWM 位周期取 T1H+T1L=1500ns）：
 *   T0H=340ns, T1H=750ns, T0L≈1160ns(由 1500−T0H，略长于手册 960ns), T1L=750ns
 *   RES≥400µs：由尾部全 0 码（CCR=0）若干周期实现，见 WS2812_RESET_SLOTS
 * 实际 CCR 在 ws2812b_init() 按 TIM4 时钟换算。
 */
extern uint32_t ws2812_code1;
extern uint32_t ws2812_code0;

/* 尾部 RES：≥400µs 低电平 = CCR=0 的 PWM 周期数（与 1500ns/位一致时 288×1.5µs=432µs） */
#define WS2812_RESET_SLOTS       (288U)
#define WS2812_RESET_ROWS        (((WS2812_RESET_SLOTS) + 23U) / 24U)
/* 最大 led_num + WS2812_RESET_ROWS，勿超 */
#define WS2812_STATE_BUF_ROWS    (32U)

#define Breath_Bright_State    0.30        //呼吸最大亮度高压采集


typedef struct
{
	uint8_t R;
	uint8_t G;
	uint8_t B;
}RGB_Color_TypeDef;

typedef enum{
    RGBLED_SHUTDOWN_MODE,
    RGBLED_LPR_MODE,
    RGBLED_KEEP_1_MODE,
    RGBLED_KEEP_2_MODE,
}rgbled_mode_t;

typedef union rgb_state_control
{
    uint8_t rgb_sta;
    struct {
        uint8_t shutdown:1;    // 关闭
        uint8_t lpr:1;         // 低功耗
        uint8_t keep_1:1;      // 保留位 1
        uint8_t keep_2:5;      // 保留位 2
    }bits;
}rgb_sta_t;



extern rgb_sta_t rgb_ctrl;
extern uint32_t color_idx;
extern uint32_t rgb_color[TOTAL_LED_COUNT];
extern uint32_t rgb_color_upload[TOTAL_LED_COUNT];
extern uint32_t rgb_color_array[COLOR_COUNT];


extern void ws2812_set_mode(rgbled_mode_t mode);
extern void ws2812_clr_mode(rgbled_mode_t mode);
extern void ws2812_shutdown(void);
extern void mode_keep1(float light);
extern void rgb_led_flush(void);
extern void ws2812_task(void *pvParameters);


void ws2812b_init(void); /* 按 TIM4 时钟计算 CCR/ARR，匹配 WS2812B-2020 时序 */
void State_Led_Show(uint16_t Pixel_Len, RGB_Color_TypeDef Color, uint8_t led_num);

#endif

