#include "pcf8563.h"
#include "pcf85063.h"
#include <stddef.h>

struct time_type__ date_time;

void pcf8563_init(void)
{
    Pcf85063_Init();
}

void pcf8563_get_cur_time(struct time_type__ *tTime)
{
    Pcf85063_DateTime_t rtc;

    if (tTime == NULL) {
        return;
    }

    if (Pcf85063_GetTime(&rtc) != 0) {
        return;
    }

    tTime->year = rtc.year;
    tTime->month = rtc.month;
    tTime->day = rtc.day;
    tTime->week = rtc.week;
    tTime->hour = rtc.hour;
    tTime->minute = rtc.minute;
    tTime->second = rtc.second;
    date_time = *tTime;
}

void pcf8563_set_cur_time(struct time_type__ *tTime)
{
    Pcf85063_DateTime_t rtc;

    if (tTime == NULL) {
        return;
    }

    rtc.year = tTime->year;
    rtc.month = tTime->month;
    rtc.day = tTime->day;
    rtc.week = tTime->week;
    rtc.hour = tTime->hour;
    rtc.minute = tTime->minute;
    rtc.second = tTime->second;
    rtc.online = 1U;

    (void)Pcf85063_SetTime(&rtc);
    date_time = *tTime;
}
