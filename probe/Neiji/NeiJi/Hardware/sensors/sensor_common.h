#ifndef NEIJI_SENSOR_COMMON_H
#define NEIJI_SENSOR_COMMON_H

#include "main.h"

/* 超过此时间未成功读数则判离线（原 5s 在 I2C 偶发失败或 UI 负载高时易误报） */
#define SENSOR_OFFLINE_MS  15000U

static inline uint8_t sensor_tick_is_stale(uint32_t last_tick, uint32_t limit_ms)
{
    uint32_t now = HAL_GetTick();

    if (last_tick == 0U) {
        return 1U;
    }
    return ((uint32_t)(now - last_tick) > limit_ms) ? 1U : 0U;
}

#endif
