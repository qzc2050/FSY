#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>

#ifndef DEVICE_CFG_SN_LEN
#define DEVICE_CFG_SN_LEN           12U
#endif

#ifndef DEVICE_CFG_MODEL_LEN
#define DEVICE_CFG_MODEL_LEN        12U
#endif

#ifndef DEVICE_CFG_DEFAULT_DEV_ADDR
#define DEVICE_CFG_DEFAULT_DEV_ADDR 1U
#endif

int DeviceConfig_Init(void);
void DeviceConfig_TaskInit(void);
uint8_t DeviceConfig_IsReady(void);

uint8_t DeviceConfig_GetDevAddr(void);
const char *DeviceConfig_GetSn(void);
const char *DeviceConfig_GetProductModel(void);
const char *DeviceConfig_GetProductName(void);

int DeviceConfig_ReadRegBlock(uint16_t start_reg, uint16_t reg_count,
                              uint8_t *out, uint16_t out_cap);
int DeviceConfig_WriteRegBlock(uint16_t start_reg, const uint8_t *data,
                               uint16_t byte_count);

void DeviceConfig_GetNetwork(uint8_t *dhcp_enable, uint8_t static_ip[4]);
void DeviceConfig_GetDoseAlarmConfig(uint32_t *hi_x100, uint32_t *lo_x100,
                                     uint32_t *alarm_enable_mask,
                                     uint8_t *alarm_volume);

int DeviceConfig_SetAlarmSound(uint8_t on);
int DeviceConfig_SetAlarmLight(uint8_t on);
void DeviceConfig_GetAlarmOutput(uint8_t *sound, uint8_t *light, uint8_t *volume);

#endif
