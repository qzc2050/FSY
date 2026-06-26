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
 * WS2812B-2020   ( PWM     T1H+T1L=1500ns)
 *   T0H=340ns, T1H=750ns, T0L1160ns( 1500 T0H   960ns), T1L=750ns
 *   RES400 s  CCR=0  PWM    (  ws2812b_init()  TIM4   )
 */
extern uint32_t ws2812_code1;
extern uint32_t ws2812_code0;

/*   RES400 s   = CCR=0  PWM     ( 1500ns/    288 1.5 s=432 s) */
#define WS2812_RESET_SLOTS       (288U)
#define WS2812_RESET_ROWS        (((WS2812_RESET_SLOTS) + 23U) / 24U)
/*   led_num + WS2812_RESET_ROWS    */
#define WS2812_STATE_BUF_ROWS    (32U)

#define Breath_Bright_State    0.30        //      


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
        uint8_t shutdown:1;    //  
        uint8_t lpr:1;         //    
        uint8_t keep_1:1;      //   1
        uint8_t keep_2:5;      //   2
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


void ws2812b_init(void); /*  TIM4   CCR/ARR   WS2812B-2020   */
void ws2812b_TaskInit(void);
void State_Led_Show(uint16_t Pixel_Len, RGB_Color_TypeDef Color, uint8_t led_num);

#endif
