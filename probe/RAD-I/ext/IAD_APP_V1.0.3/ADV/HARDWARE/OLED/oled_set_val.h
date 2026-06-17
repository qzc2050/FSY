#ifndef __MENU_CTRL_H
#define __MENU_CTRL_H

#include <stdint.h>
#include <stdbool.h>

extern struct time_type__ data_time;


void set_rth(void);
void set_dth(void);
void Set_Val_Up(uint8_t min,uint8_t max,char *str);
void Set_Val_Down(uint8_t min,uint8_t max,char *str);
void Set_Val_Inc(void);
void Set_Val_Sub(void);
void Save_Val(void);
void Set_Val_Limit(void);
void Set_Val_Re(void);

#endif


