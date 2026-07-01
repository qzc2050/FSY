#include "lcd_backlight.h"

#include "tim.h"

void LcdBacklight_SetPercent(float percent)
{
    if (percent < 0.0f) {
        percent = 0.0f;
    } else if (percent > 100.0f) {
        percent = 100.0f;
    }
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, percent * 1000.0f / 100.0f);
}

void LcdBacklight_ApplyDisplayEnable(uint8_t enable, float bright_sz)
{
    if (enable != 0U) {
        LcdBacklight_SetPercent(bright_sz);
    } else {
        LcdBacklight_SetPercent(0.0f);
    }
}
