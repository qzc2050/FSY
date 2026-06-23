#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "main.h"

#define DEVICE_ID       "W5500"
#define FW_VER_HIGH     1U
#define FW_VER_LOW      0U

void set_w5500_network(void);
void set_w5500_default(void);
void Reset_W5500(void);
void W5500_Hw_Prepare(void);
void W5500_SpiMutexInit(void);
void W5500_SpiRecover(void);
void W5500_SpiGetStats(uint32_t *ok, uint32_t *err, uint32_t *last_st, uint32_t *last_ecode);
uint8_t W5500_SpiRawReadVersion(void);

#endif
