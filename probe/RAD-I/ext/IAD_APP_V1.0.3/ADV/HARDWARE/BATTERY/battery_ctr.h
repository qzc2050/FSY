#ifndef __BATTERY_CTR_H
#define __BATTERY_CTR_H

#include <stdint.h>
#include <stdbool.h>

bool Low_Battery_Judge(float vol,bool cmd);
uint8_t Battery_Get_Percent(void);
extern void Battery_Detect(bool ref);
void Power_Test(void);

#endif

