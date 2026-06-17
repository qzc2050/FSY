/**********************************************************************************************************
 * 文件名：key.h
 * 概  述：按键驱动头文件
 * 创建时间：2026-04-20
 * 更新时间：2026-04-20
 * 作  者：Mr.Liu
 * 版  本：1.0.0
 * Copyright (c) 2026, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/**
 * @file key.h
 * @brief 按键驱动接口
 * 
 * @note 按键引脚定义：
 *       - KEY_RETURN : PA4  (返回键)
 *       - KEY_UP     : PH7  (上键)
 *       - KEY_DOWN   : PC5  (下键)
 *       - KEY_OK     : PC4  (确认键)
 */

#ifndef __KEY_H
#define __KEY_H

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/**********************************************************************************************************
 *                                  按键引脚定义
 **********************************************************************************************************/
#define KEY_RETURN_GPIO_Port        GPIOA
#define KEY_RETURN_GPIO_Pin         GPIO_PIN_4

#define KEY_UP_GPIO_Port            GPIOH
#define KEY_UP_GPIO_Pin             GPIO_PIN_7

#define KEY_DOWN_GPIO_Port          GPIOC
#define KEY_DOWN_GPIO_Pin           GPIO_PIN_5

#define KEY_OK_GPIO_Port            GPIOC
#define KEY_OK_GPIO_Pin             GPIO_PIN_4

/**********************************************************************************************************
 *                                  按键状态定义
 **********************************************************************************************************/
typedef enum {
    KEY_STATE_RELEASED = 0,       /* 按键释放 */
    KEY_STATE_PRESSED,            /* 按键按下 */
    KEY_STATE_LONG_PRESS          /* 按键长按 */
} KEY_State_t;

/**********************************************************************************************************
 *                                  按键 ID 定义
 **********************************************************************************************************/
typedef enum {
    KEY_ID_RETURN = 0,            /* 返回键 */
    KEY_ID_UP,                    /* 上键 */
    KEY_ID_DOWN,                  /* 下键 */
    KEY_ID_OK,                    /* 确认键 */
    KEY_ID_MAX                    /* 按键总数 */
} KEY_ID_t;

/**********************************************************************************************************
 *                                  按键数据结构
 **********************************************************************************************************/
typedef struct {
    GPIO_TypeDef *port;           /* GPIO 端口 */
    uint16_t pin;                 /* GPIO 引脚 */
    bool last_state;              /* 上一次按键状态 */
    bool current_state;           /* 当前按键状态 */
    uint32_t press_time;          /* 按下持续时间 (ms) */
    uint32_t last_press_time;     /* 上次按下时间 */
    KEY_State_t state;            /* 按键状态 */
    bool trigger_flag;            /* 触发标志 */
} KEY_Handle_t;

/**********************************************************************************************************
 *                                  函数声明
 **********************************************************************************************************/
/* 初始化函数 */
void KEY_Init(void);

/* 按键扫描函数 (需周期性调用) */
void KEY_Scan(void);

/* 按键状态获取函数 */
bool KEY_IsPressed(KEY_ID_t key_id);              /* 检测按键是否按下 */
bool KEY_IsReleased(KEY_ID_t key_id);             /* 检测按键是否释放 */
bool KEY_IsLongPress(KEY_ID_t key_id);            /* 检测按键是否长按 */
KEY_State_t KEY_GetState(KEY_ID_t key_id);        /* 获取按键状态 */

/* 获取按键 ID */
KEY_ID_t KEY_GetPressedKey(void);                 /* 获取当前按下的按键 ID */

/* 调试函数 */
void KEY_PrintStatus(void);                       /* 打印按键状态 */

#endif /* __KEY_H */
