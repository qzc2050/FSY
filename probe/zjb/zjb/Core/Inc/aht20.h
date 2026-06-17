#ifndef __AHT20_H
#define __AHT20_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  float temperature_c;
  float humidity_rh;
  uint8_t  online;
  uint32_t last_update_tick;
} AHT20_Data_t;

void AHT20_Init(void);
void AHT20_Update(void);
void AHT20_GetData(AHT20_Data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_H */

