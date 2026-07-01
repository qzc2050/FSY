#ifndef LCD_BACKLIGHT_H
#define LCD_BACKLIGHT_H

#include <stdint.h>

/** 背光 PWM 0~100% */
void LcdBacklight_SetPercent(float percent);

/** bit14=1 时用 bright_sz，=0 时关背光 */
void LcdBacklight_ApplyDisplayEnable(uint8_t enable, float bright_sz);

#endif
