#ifndef NEIJI_BMP280_H
#define NEIJI_BMP280_H

#include <stdint.h>

typedef struct {
    float temperature_c;
    float pressure_pa;
    uint8_t online;
    uint32_t last_update_tick;
} BMP280_Data_t;

void BMP280_Init(void);
void BMP280_Update(void);
void BMP280_GetData(BMP280_Data_t *out);

#endif
