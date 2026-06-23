#ifndef NEIJI_SENSOR_TASK_H
#define NEIJI_SENSOR_TASK_H

#include <stdint.h>

#include "pcf85063.h"

typedef struct {
    float temperature;
    float humidity;
    float baro;
    uint16_t CO2;
    uint16_t PM2_5;
    Pcf85063_DateTime_t dt;
} Environment_Data_t;

extern Environment_Data_t env_data;

void Sensor_TaskInit(void);

#endif
