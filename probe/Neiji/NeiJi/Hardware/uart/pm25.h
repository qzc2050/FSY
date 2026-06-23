#ifndef NEIJI_PM25_H
#define NEIJI_PM25_H

#include <stdint.h>

typedef struct {
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm10;
    uint8_t online;
    uint32_t last_update_tick;
} PM25_Data_t;

void PM25_Init(void);
void PM25_ProcessByte(uint8_t byte);
void PM25_GetData(PM25_Data_t *out);
void PM25_Rx_Start(void);
void PM25_OnRxCplt(void);

#endif
