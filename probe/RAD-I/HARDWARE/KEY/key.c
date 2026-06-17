/**********************************************************************************************************
 * 文件名：key.c
 * 概  述：按键驱动程序
 * 创建时间：2026-04-20
 * 更新时间：2026-04-20
 * 作  者：Mr.Liu
 * 版  本：1.0.0
 * Copyright (c) 2026, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/**
 * @file key.c
 * @brief 按键驱动实现
 * 
 * @note 按键扫描周期建议：10-20ms
 * @note 去抖动时间：20ms
 * @note 长按时间：1000ms
 */

#include "key.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/**********************************************************************************************************
 *                                  宏定义
 **********************************************************************************************************/
#define KEY_DEBOUNCE_TIME       20      /* 去抖动时间 (ms) */
#define KEY_LONG_PRESS_TIME     1000    /* 长按时间 (ms) */
#define KEY_SCAN_INTERVAL       10      /* 扫描间隔 (ms) */

/**********************************************************************************************************
 *                                  全局变量
 **********************************************************************************************************/
static KEY_Handle_t key_handles[KEY_ID_MAX];
//static uint32_t key_tick_start = 0;

/**********************************************************************************************************
 *                                  内部函数声明
 **********************************************************************************************************/
static void KEY_UpdateState(KEY_Handle_t *handle);
static bool KEY_ReadPin(KEY_Handle_t *handle);
static uint32_t KEY_GetTick(void);

/**********************************************************************************************************
 *                                  初始化函数
 **********************************************************************************************************/
/**
 * @brief 按键初始化
 */
void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 使能 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    /* 初始化按键结构体 */
    memset(key_handles, 0, sizeof(key_handles));
    
    /* 配置返回键 PA4 */
    key_handles[KEY_ID_RETURN].port = KEY_RETURN_GPIO_Port;
    key_handles[KEY_ID_RETURN].pin = KEY_RETURN_GPIO_Pin;
    
    /* 配置上键 PH7 */
    key_handles[KEY_ID_UP].port = KEY_UP_GPIO_Port;
    key_handles[KEY_ID_UP].pin = KEY_UP_GPIO_Pin;
    
    /* 配置下键 PC5 */
    key_handles[KEY_ID_DOWN].port = KEY_DOWN_GPIO_Port;
    key_handles[KEY_ID_DOWN].pin = KEY_DOWN_GPIO_Pin;
    
    /* 配置确认键 PC4 */
    key_handles[KEY_ID_OK].port = KEY_OK_GPIO_Port;
    key_handles[KEY_ID_OK].pin = KEY_OK_GPIO_Pin;
    
    /* 配置所有按键引脚为输入模式，带上拉电阻 */
    /* PA4 - 返回键 */
    GPIO_InitStruct.Pin = KEY_RETURN_GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(KEY_RETURN_GPIO_Port, &GPIO_InitStruct);
    
    /* PH7 - 上键 */
    GPIO_InitStruct.Pin = KEY_UP_GPIO_Pin;
    HAL_GPIO_Init(KEY_UP_GPIO_Port, &GPIO_InitStruct);
    
    /* PC5 - 下键 */
    GPIO_InitStruct.Pin = KEY_DOWN_GPIO_Pin;
    HAL_GPIO_Init(KEY_DOWN_GPIO_Port, &GPIO_InitStruct);
    
    /* PC4 - 确认键 */
    GPIO_InitStruct.Pin = KEY_OK_GPIO_Pin;
    HAL_GPIO_Init(KEY_OK_GPIO_Port, &GPIO_InitStruct);
    
    /* 记录初始时间 */
//    key_tick_start = KEY_GetTick();
    
    printf("[KEY] 按键初始化完成\r\n");
//    printf("    - RETURN: PA4\r\n");
//    printf("    - UP:     PH7\r\n");
//    printf("    - DOWN:   PC5\r\n");
//    printf("    - OK:     PC4\r\n");
}

/**********************************************************************************************************
 *                                  按键扫描函数
 **********************************************************************************************************/
/**
 * @brief 按键扫描 (需周期性调用，建议 10-20ms)
 */
void KEY_Scan(void)
{
    for (uint8_t i = 0; i < KEY_ID_MAX; i++)
        KEY_UpdateState(&key_handles[i]);
}

/**********************************************************************************************************
 *                                  按键状态更新
 **********************************************************************************************************/
/**
 * @brief 更新单个按键状态
 * @param handle: 按键句柄
 */
static void KEY_UpdateState(KEY_Handle_t *handle)
{
    bool current_pin_state = KEY_ReadPin(handle);
    uint32_t current_tick = KEY_GetTick();
    
    /* 保存上一次状态 */
    handle->last_state = handle->current_state;
    handle->current_state = current_pin_state;
    
    /* 检测按键按下 (低电平有效) */
    if (current_pin_state == false) {
        /* 如果之前是释放状态，现在是按下，记录按下时间 */
        if (handle->last_state == true) {
            handle->last_press_time = current_tick;
            handle->press_time = 0;
            handle->state = KEY_STATE_PRESSED;
            handle->trigger_flag = true;
        } else {
            /* 计算按下持续时间 */
            handle->press_time = current_tick - handle->last_press_time;
            
            /* 检测是否达到长按时间 */
            if (handle->press_time >= KEY_LONG_PRESS_TIME) {
                handle->state = KEY_STATE_LONG_PRESS;
            }
        }
    } else {
        /* 按键释放 */
        if (handle->last_state == false) {
            /* 从按下到释放，检测是否是短按 */
            if (handle->press_time < KEY_LONG_PRESS_TIME) {
                handle->state = KEY_STATE_RELEASED;
                handle->trigger_flag = true;
            }
        } else {
            handle->state = KEY_STATE_RELEASED;
            handle->trigger_flag = false;
        }
        handle->press_time = 0;
    }
}

/**
 * @brief 读取按键引脚状态
 * @param handle: 按键句柄
 * @return true: 释放 (高电平), false: 按下 (低电平)
 */
static bool KEY_ReadPin(KEY_Handle_t *handle)
{
    return (HAL_GPIO_ReadPin(handle->port, handle->pin) == GPIO_PIN_SET);
}

/**
 * @brief 获取系统 tick
 * @return 当前 tick 值 (ms)
 */
static uint32_t KEY_GetTick(void)
{
    return HAL_GetTick();
}

/**********************************************************************************************************
 *                                  按键状态获取函数
 **********************************************************************************************************/
/**
 * @brief 检测按键是否按下 (短按或长按)
 * @param key_id: 按键 ID
 * @return true: 按下，false: 释放
 */
bool KEY_IsPressed(KEY_ID_t key_id)
{
    if (key_id >= KEY_ID_MAX) {
        return false;
    }
    return (key_handles[key_id].current_state == false);
}

/**
 * @brief 检测按键是否释放
 * @param key_id: 按键 ID
 * @return true: 释放，false: 按下
 */
bool KEY_IsReleased(KEY_ID_t key_id)
{
    if (key_id >= KEY_ID_MAX) {
        return true;
    }
    return (key_handles[key_id].current_state == true);
}

/**
 * @brief 检测按键是否长按
 * @param key_id: 按键 ID
 * @return true: 长按，false: 未长按
 */
bool KEY_IsLongPress(KEY_ID_t key_id)
{
    if (key_id >= KEY_ID_MAX) {
        return false;
    }
    return (key_handles[key_id].state == KEY_STATE_LONG_PRESS);
}

/**
 * @brief 获取按键状态
 * @param key_id: 按键 ID
 * @return 按键状态
 */
KEY_State_t KEY_GetState(KEY_ID_t key_id)
{
    if (key_id >= KEY_ID_MAX) {
        return KEY_STATE_RELEASED;
    }
    return key_handles[key_id].state;
}

/**
 * @brief 获取当前按下的按键 ID
 * @return 按下的按键 ID，如果没有按键按下则返回 KEY_ID_MAX
 */
KEY_ID_t KEY_GetPressedKey(void)
{
    for (uint8_t i = 0; i < KEY_ID_MAX; i++) {
        if (key_handles[i].current_state == false) {
            return (KEY_ID_t)i;
        }
    }
    return KEY_ID_MAX;
}

/**********************************************************************************************************
 *                                  调试函数
 **********************************************************************************************************/
/**
 * @brief 打印按键状态 (调试用)
 */
void KEY_PrintStatus(void)
{
    printf("\r\n========== KEY Status ==========\r\n");
    printf("  RETURN (PA4): %s\r\n", KEY_IsPressed(KEY_ID_RETURN) ? "PRESSED" : "RELEASED");
    printf("  UP     (PH7): %s\r\n", KEY_IsPressed(KEY_ID_UP) ? "PRESSED" : "RELEASED");
    printf("  DOWN   (PC5): %s\r\n", KEY_IsPressed(KEY_ID_DOWN) ? "PRESSED" : "RELEASED");
    printf("  OK     (PC4): %s\r\n", KEY_IsPressed(KEY_ID_OK) ? "PRESSED" : "RELEASED");
    printf("=================================\r\n");
}
