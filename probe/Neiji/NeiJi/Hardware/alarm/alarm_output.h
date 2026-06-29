#ifndef ALARM_OUTPUT_H
#define ALARM_OUTPUT_H

#include <stdint.h>

/** 灯带 / UI 共用的视觉状态（优先级：故障 > 高超限 > 低超限 > 正常） */
typedef enum {
    ALARM_VIS_FAULT = 0,
    ALARM_VIS_HI,
    ALARM_VIS_LO,
    ALARM_VIS_NORMAL,
} Alarm_Visual_State_t;

void Alarm_Output_Init(void);
void Alarm_Output_Update(void);

Alarm_Visual_State_t Alarm_Output_GetVisualState(void);

/** 供 UI 任务读取：剂量率超上限（辐射 bit0） */
uint8_t Alarm_Output_IsDoseHiActive(void);

/** geiger 任务每秒上报 CPS，用于本底长时间无计数故障判定 */
void Alarm_Output_NotifyCps(uint32_t cps);

#endif
