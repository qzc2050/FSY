#ifndef __MUTI_MENU_H
#define __MUTI_MENU_H

#include "stm32l0xx.h"

struct menu_struct
{
  uint8_t index_val;     //索引值
  uint8_t key1_single;   //短按时要切换的索引值
  uint8_t key2_single;   //短按时要切换的索引值
  uint8_t key0_single;   //短按时要切换的索引值
//	uint8_t key1_double;   //双击时要切换的索引值
//	uint8_t key2_double;   //双击时要切换的索引值
  int (* point)(void);   //函数指针，输入值为空，返回类型为int
};
typedef struct menu_struct MenuStruct;

void OLED_SHOW_ARROW(uint8_t ADDR_x,uint8_t ADDR_y);
void OLED_Draw_Border_Line(void);
void USB_Detect(void);
void Base_Oper(void);
void menu_home_1(void);
void menu_home_2(void);
void Menu_Home(uint8_t addr_x,uint8_t addr_y);
void Menu_TH_Set(uint8_t addr_x,uint8_t addr_y);
void Menu_TH_Dose_Set(uint8_t addr_x,uint8_t addr_y);
void Menu_Clr_Day_Data(uint8_t addr_x,uint8_t addr_y);
void Menu_Clr_Crt_Dose(uint8_t addr_x,uint8_t addr_y);
void Menu_Sys_Set(uint8_t addr_x,uint8_t addr_y);
void Menu_Display_Set(uint8_t addr_x,uint8_t addr_y);
void Progress_Bar_Draw(void);
void Progress_Bar_Clear(void);
void set_bar_intf(void);
void Progress_Bar_Up(void);
void Progress_Bar_Sub(void);
void Set_Bright_Grade(uint8_t write_sta);
void Menu_Reset(uint8_t addr_x,uint8_t addr_y);
void Set_Sc_Extinct_Time(uint8_t write_sta);
void sys_info_1(void);
void sys_info_2(void);
void sys_info_3(void);
#endif

