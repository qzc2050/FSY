#ifndef __BEEP_H
#define __BEEP_H


#include <stdint.h>
#include <stdbool.h>

#include "tim.h"


typedef enum{
    BEEP_EVENT_NULL,        // 无报警事件
    
    /* 添加报警事件（值越大，优先级越高） */
    BEEP_EVENT_RTH,         // 超剂量率事件
//    BEEP_EVENT_RTH_EXIT,    // 退出超剂量率事件
//    BEEP_EVENT_DTH,         // 超剂量事件（当日剂量值、当前剂量值）
    BEEP_EVENT_LIMIT,       // 超上限事件（剂量率、当前剂量值）
    BEEP_EVENT_CLR,         // 清除剂量/剂量率报警事件
//    BEEP_EVENT_TIMING,      // 计时模式启动事件
//    BEEP_EVENT_LB,          // 低电量事件
    BEEP_EVENT_SETTING,     // 蜂鸣器设置测试事件
    BEEP_EVENT_TEST,        // 蜂鸣器测试事件
    BEEP_EVENT_STOP_TEST,   // 蜂鸣器测试停止事件
    // ...
}BEEP_EVENT;

#define BEEP_STA_OFF    false
#define BEEP_STA_ON     true

#define BEEP_DUTY           800
#define ALARM_LED_ON()      HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_RESET);
#define ALARM_LED_OFF()     HAL_GPIO_WritePin(LED_ALARM_GPIO_Port, LED_ALARM_Pin, GPIO_PIN_SET);



extern void Beep_On(void);
extern void Beep_Off(void);
static bool Beep_Alternate(uint16_t time,uint8_t cnt,bool ref);

extern uint8_t beep_event;
extern void Beep_Ctr(uint8_t req_event);
void Beep_PinEnsure(void);
void Beep_DebugProbe(void);


#endif


