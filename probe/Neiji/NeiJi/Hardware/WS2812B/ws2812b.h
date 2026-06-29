#ifndef _WS2812B_H_
#define _WS2812B_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "main.h"

#define COLOR_COUNT         21
#define TOTAL_LED_COUNT     8
#define COMMON_DELAY        20

extern uint32_t ws2812_code1;
extern uint32_t ws2812_code0;

#define WS2812_RESET_SLOTS       (288U)
#define WS2812_RESET_ROWS        (((WS2812_RESET_SLOTS) + 23U) / 24U)
#define WS2812_STATE_BUF_ROWS    (32U)

#define Breath_Bright_State    0.30f

typedef struct {
    uint8_t R;
    uint8_t G;
    uint8_t B;
} RGB_Color_TypeDef;

typedef enum {
    WS2812_PATTERN_OFF = 0,
    WS2812_PATTERN_WHITE_SLOW,
    WS2812_PATTERN_RED_FAST,
    WS2812_PATTERN_RED_SLOW,
    WS2812_PATTERN_RED_SOLID,
} ws2812_pattern_t;

void ws2812b_init(void);
void ws2812b_TaskInit(void);
void ws2812_set_pattern(ws2812_pattern_t pattern);
ws2812_pattern_t ws2812_get_pattern(void);
void rgb_led_flush(void);

#endif
