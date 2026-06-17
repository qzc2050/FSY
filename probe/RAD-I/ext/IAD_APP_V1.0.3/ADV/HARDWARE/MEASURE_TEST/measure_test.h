#ifndef __MEASURE_TEST_H
#define __MEASURE_TEST_H

#include <stdio.h>
#include "stm_flash.h"

#define MEAS_DATA_WRITE_BASE_ADD       0x0800FCE0    //数据保存的初始地址
#define MEAS_FLASH_SAVE_DATA_NUM       50
#define MEAS_DATA_WRITE_MAX_ADDR       MEAS_DATA_WRITE_BASE_ADD + (MEAS_FLASH_SAVE_DATA_NUM - 1) * 16

/* 数据在FLASH的存储地址 */
#define FLASH_WriteAddress    				 0x0800FCE0    //写在靠后位置，防止破坏程序
#define FLASH_ReadAddress     				 FLASH_WriteAddress
//#define FLASH_TESTSIZE        				 45 * 4
#define FLASH_TESTSIZE        				 MEAS_FLASH_SAVE_DATA_NUM * 4

#define MEAS_INIT_TIME_ADD             0x080807AC    //临时测量模式起始时分秒
#define MEAS_VALID_DATA_NUM_ADD        0x080807A8    //临时测量模式已保存数据组数的地址
#define MEAS_REC_ONE_ROUND_ADD         0x080807A4
#define MEAS_SAVE_DATA_NUM_ADD         0x080807A0



typedef struct Meas_Data_var
{
	 uint32_t meas_rec_offset_num;     //历史记录查看的偏移地址
	 uint32_t meas_data_offset_num;    //覆盖保存到flash的数据个数（当前数据保存在flash的偏移位置））
	 uint32_t meas_valid_data_num;     //当前保存到flash的有效数据个数
	 
} Meas_Data_var_st, *Meas_Data_var_pst;

typedef struct Meas_Opration_Sta
{
	uint32_t meas_rec_one_round;      //只能是32位，否则可能进入硬件错误
} Meas_Opration_Sta_st, *Meas_Opration_Sta_pst;


uint8_t flash_test(void);
void Meas_Base_Init(void);
void Meas_Recovery_Factory(void);
void Meas_Data_Num_Add(void);
int  Meas_Test_Mode(void);
void Meas_Usart_Time_Print(uint32_t time);
void Meas_Usart_Aver_Print(float dose_rate);
void Meas_Print_History(void);
void Meas_Print_dose_rate(void);
#endif

