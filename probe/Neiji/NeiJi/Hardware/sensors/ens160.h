#ifndef NEIJI_ENS160_H
#define NEIJI_ENS160_H

#include <stdint.h>

typedef struct {
    uint16_t eco2;
    uint16_t tvoc;
    uint8_t online;
    uint8_t device_status;
    uint8_t warmup_phase;
    uint32_t last_update_tick;
} ENS160_Data_t;

void ENS160_Init(void);
void ENS160_SetCompensation(float temp_c, float rh_percent);
void ENS160_Update(void);
void ENS160_GetData(ENS160_Data_t *out);

#endif
