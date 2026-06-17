#ifndef __OLED_RTC_TIME_H
#define __OLED_RTC_TIME_H

#include <stdbool.h>

// 定义日期数值增减
enum {
    TIME_VAL_DOWN,     // 数值减小
    TIME_VAL_UP        // 数值增大
};

void set_datatime(void);
void Set_Time_Val(bool ctr);
void SAVE_RTC_TIME(void);


#endif


