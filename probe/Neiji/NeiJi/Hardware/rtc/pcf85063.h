#ifndef NEIJI_PCF85063_H
#define NEIJI_PCF85063_H

#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t online;
} Pcf85063_DateTime_t;

void Pcf85063_Init(void);
int Pcf85063_GetTime(Pcf85063_DateTime_t *out);
int Pcf85063_SetTime(const Pcf85063_DateTime_t *dt);

#endif
