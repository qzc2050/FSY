#ifndef __CONTROL_H
#define __CONTROL_H

#include "stm32l0xx_hal.h"

#include "tim.h"
#include "key.h"
#include "cmd.h"
#include "oled.h"
#include "main.h"
#include "usart.h"
#include "myiic.h"
#include "pcf8563.h"
#include "max17048.h"
#include "oledfont.h"
#include "oled_data.h"
#include "stm_flash.h"
#include "muti_menu.h"
#include "timing_mode.h"
#include "battery_ctr.h"
#include "oled_set_val.h"
#include "oled_rtc_time.h"
#include "low_power_run.h"


#include "dose_rate.h"
#include "sys_ctr.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define KEY_DEBUG  0
#if KEY_DEBUG
#define KEY_DEBUG_PRINTF(n,args...)  printf(n,##args)
#else
#define KEY_DEBUG_PRINTF(n,args...)
#endif

#define  __CTR_EXTERN  extern


#define READ_USB            HAL_GPIO_ReadPin(USB_DET_GPIO_Port,USB_DET_Pin)     //读取USB接口状态
#define BLUE_LED_ON()       HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET)
#define BLUE_LED_OFF()      HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET)


#define LONG_KET_TIME_BASE      25          // 按键长按时间

#define LPR_LED_TWINKLE_TIME    5000        // LED闪烁时间,单位：ms

#define DAY_PER_SAVE_TIME       900000      // 当日数据保存一次的时间间隔，单位：ms

#define LOW_BAT_VOL             3100        //低电量时的电池电压

#define RATE_LIMIT              10000       // 剂量率上限 10.00 mSv/h = 10000 uSv/h
#define DOSE_LIMIT              99990000    // 剂量值上限 99.99 Sv  = 99990000 uSv


/*-----------数据采集相关-----------*/
#define SAMPLE_PERIOD           100  // 采集时间间隔，单位：ms
/*-----------数据采集相关-----------*/


typedef enum 
{
    POWER_ON = 0,    //开机
    POWER_OFF,       //关机
}Power_sta;

typedef enum 
{
    ALARM_BRIGHT = 0,   //报警亮屏
    KEY_BRIGHT,         //按键亮屏
}Sys_Run_sta;

typedef enum 
{ 
    AGING_OFF = 0,      //关闭老化测试
    KEEPING_BRIGHT,     //保持亮屏
    KEEPING_LPR,        //低功耗
}Aging_sta;

typedef enum
{ 
    KEEPING_POWER_ON = 0,    //保持开机
    REQUEST_POWER_OFF,       //请求关机
}Request_sta;

typedef enum 
{
    LPR_MODE = 0,    //低功耗运行模式
    RUN_MODE,        //正常运行模式
    EXIT_LPR_MODE,   //退出低功耗模式
}Sys_Mode;


extern bool ref_sta;
extern char str_temp[18];


void Real_Data_deal(void);
void Sum_Dose(void);
void Get_tim_cnt(void);
void Data_Cal(void);
void Exit_LPR(bool sta);
void Data_Save_Cnt_Add(void);
void Flash_Save_Day_Data(void);
void Update_DayData_To_EEPROM(bool sta);
void Update_CrtData_To_EEPROM(bool sta);
void Gap_Save_Data(void);
void Data_Refresh(bool ref);
void Gap_Execute(void);
void Icon_Refresh(void);
void Request_Twinkle(uint8_t x, uint8_t y, char *str, uint8_t len,bool update);
void Sys_Reset(void);
uint8_t Detect_first_use(void);
void Decrease_Offset_Position(void);
void Increase_Offset_Position(void);
int16_t Adjust_Offset_Position(uint32_t date);
void Flash_Save_Test(void);
extern void Aging_Test(void);
#endif

