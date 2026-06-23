#ifndef NEIJI_PCF8563_SHIM_H
#define NEIJI_PCF8563_SHIM_H

#include <stdint.h>
#include <stdbool.h>

typedef struct time_type__ {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DateTime_t;

extern struct time_type__ date_time;

void pcf8563_init(void);
void pcf8563_set_cur_time(struct time_type__ *tTime);
void pcf8563_get_cur_time(struct time_type__ *tTime);

#endif
