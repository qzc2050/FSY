#ifndef __OLED_CTRL_H
#define __OLED_CTRL_H

#include "stm32l0xx_hal.h"
#include "pcf8563.h"
#include "sys_ctr.h"

void Unit_Show(uint8_t addr_val_x,uint8_t addr_val_y,uint8_t val_size,\
	           uint8_t addr_uint_x,uint8_t addr_uint_y,uint8_t uint_size,float fdose,uint8_t mode);
bool Dose_To_Str(bool data_type);
void OLED_Dose_Show(uint8_t addr_val_x,uint8_t addr_val_y,uint8_t val_size,float fdose);
void Clr_Crt_Dose(void);
void Show_Date_Time(uint8_t x,uint8_t y,uint32_t date);
uint8_t Rec_Mode_Get_Valid_Page(void);
void His_Page_Home(void);
void His_Page_Up(void);
void His_Page_Down(void);
void History_tip_keep(void);
void Clr_Day_Data(void);
void Adjust_History(uint32_t date_temp);
#endif

