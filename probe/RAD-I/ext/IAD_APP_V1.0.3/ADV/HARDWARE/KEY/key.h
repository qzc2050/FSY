#ifndef __KEY_H
#define __KEY_H

#include "stm32l0xx.h"
#include "gpio.h"
#include <stdlib.h>


// 端口设置
#define BTN0	HAL_GPIO_ReadPin(KEY_INT_GPIO_Port,KEY_INT_Pin)		// 按键状态读取
#define BTN1	HAL_GPIO_ReadPin(KEY2_GPIO_Port,KEY2_Pin)		// 按键状态读取
#define BTN2	HAL_GPIO_ReadPin(KEY1_GPIO_Port,KEY1_Pin)		// 按键状态读取


// 按键控制
typedef enum 
{
	KEY_PC,
    KEY_L,
    KEY_R,
    KEY_NUM
}KEYIDX;

// 按键控制
typedef enum 
{
	KeyDown,
	KeyUp,
	KeyShort,
	KeyLong,
	KeyDouble,
	KeyNull,
	KeyLongUp,    // ======无操作======
}KEYSTATE;

typedef struct BtnControl
{
	uint8_t	  KeyStep:3;      // 按键状态（枚举BtnStaSingle）
	uint8_t	  KeyState:2;
	uint8_t	  KeyStatus:3;
	uint16_t  DoubleTimes;    // 双击计时值
	uint16_t  DownTimes;      // 按下计时值
	uint8_t   Key_Long_Sta;
} BtnControl_st;

extern __IO BtnControl_st key_s[KEY_NUM];

void Btn_GPIO_Init(void);
void Key_Func(void);
//void KeyShort_OP(uint8_t keyx);
void KeyDouble_OP(uint8_t keyx);
void Key_PC_Long(void);
void Key_PC_Double(void);
void Key_L_Long(void);
void Key_R_Long(void);
void KeyOperate(void);
void Power_Off_Operation(void);
void Sys_Shutdown(void);
void BtnOpenPower(void);
void KeyScan(BtnControl_st *pBtn, uint8_t bPin);
#endif

