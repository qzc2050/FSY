#ifndef __ENS160_H
#define __ENS160_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint16_t eco2;
  uint16_t tvoc;
  uint8_t  online;
  uint8_t  device_status;
  uint8_t  warmup_phase;     /* 0=normal 1=warmup 2=first-startup 3=invalid */
  uint32_t last_update_tick;
} ENS160_Data_t;

void ENS160_Init(void);
void ENS160_SetCompensation(float temp_c, float rh_percent);
void ENS160_Update(void);
void ENS160_GetData(ENS160_Data_t *out);
const char *ENS160_WarmupText(uint8_t phase);

#ifdef __cplusplus
}
#endif

#endif /* __ENS160_H */
