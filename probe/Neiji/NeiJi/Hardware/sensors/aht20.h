#ifndef NEIJI_AHT20_H
#define NEIJI_AHT20_H

#include <stdint.h>

typedef struct {
    float temperature_c;
    float humidity_rh;
    uint8_t online;
    uint32_t last_update_tick;
} AHT20_Data_t;

void AHT20_Init(void);
void AHT20_Update(void);
void AHT20_GetData(AHT20_Data_t *out);

#endif
