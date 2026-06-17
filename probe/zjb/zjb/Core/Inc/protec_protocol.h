#ifndef __PROTEC_PROTOCOL_H
#define __PROTEC_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PROTEC_ADDR  0xEFU

typedef struct
{
  uint32_t dose_rate;    /* reg 1: 辐射量*100，本板暂无，填0 */
  uint32_t temp;         /* reg 3: 温度*10 (℃) */
  uint32_t press;        /* reg 5: 气压 (Pa) */
  uint32_t hum;          /* reg 7: 湿度 (%) */
  uint32_t co2;          /* reg 9: CO2 (ppm) */
  uint32_t pm2d5;        /* reg 11: PM2.5*10 (ug/m3) */
  uint32_t alarm_bit1;   /* reg 13: 报警状态 bit */
  uint32_t io_status;    /* reg 15: I/O 状态 bit0~bit11 */
  uint32_t reserve1;     /* reg 17: 预留 */
  uint32_t reserve2;     /* reg 19: 预留 */
  uint32_t reserve3;     /* reg 21: 预留 */
} SystemStatus_t;

extern SystemStatus_t g_system_status;
extern const char g_sw_version[20];

void Protec_Init(void);
void Protec_SendRealtime(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTEC_PROTOCOL_H */

