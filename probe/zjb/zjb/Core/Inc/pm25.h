#ifndef __PM25_H
#define __PM25_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  uint16_t pm1_0;
  uint16_t pm2_5;
  uint16_t pm10;
  uint8_t  online;
  uint32_t last_update_tick;
} PM25_Data_t;

void PM25_Init(void);
void PM25_ProcessByte(uint8_t byte);
void PM25_GetData(PM25_Data_t *out);
void PM25_Rx_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* __PM25_H */


