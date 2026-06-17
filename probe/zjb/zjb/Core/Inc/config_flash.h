#ifndef __CONFIG_FLASH_H
#define __CONFIG_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* STM32F103CBT6: 128KB Flash, last page (1KB) at 0x0801FC00 */
#define CONFIG_FLASH_ADDR   0x0801FC00U
#define CONFIG_MAGIC        0x32535946U  /* 'FYS2' 任意固定值 */
#define CONFIG_VERSION      0x00010001U

typedef struct
{
  uint32_t magic;
  uint32_t version;

  uint32_t dose_up;
  uint32_t dose_down;
  uint32_t temp_up;
  uint32_t temp_down;
  uint32_t press_up;
  uint32_t press_down;
  uint32_t hum_up;
  uint32_t hum_down;
  uint32_t co2_up;
  uint32_t co2_down;
  uint32_t pm25_up;
  uint32_t pm25_down;

  uint32_t alarm_enable;

  uint16_t address;
  uint16_t control_bit;

  uint32_t control_bit2;      /* 对应 io_status 的 IO 输出默认值（上电按此初始化） */

  char     serialnum[16];

  uint32_t reserve[8];

  uint32_t crc;               /* 覆盖以上所有字段(不含 crc)的 CRC32 */
} SystemConfig_t;

extern SystemConfig_t g_config;

void Config_Load(void);
void Config_Save(void);
void Config_SetDefault(void);
void Config_ApplyIoOutputs(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_FLASH_H */

