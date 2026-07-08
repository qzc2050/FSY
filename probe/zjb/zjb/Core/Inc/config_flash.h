#ifndef __CONFIG_FLASH_H
#define __CONFIG_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* STM32F103CBT6: 128KB Flash, last page (1KB) at 0x0801FC00 */
#define CONFIG_FLASH_ADDR   0x0801FC00U
#define CONFIG_MAGIC        0x32535946U  /* 'FYS2' 任意固定值 */
#define CONFIG_VERSION      0x00010004U
#define CONFIG_VERSION_V3   0x00010003U
#define CONFIG_VERSION_V2   0x00010002U
#define CONFIG_VERSION_V1   0x00010001U

/** reg123 control_bit2：bit13/14 与内机一致；bit9=LoRa 电源+桥接总开关 */
#define CTRL2_BIT_PM25_POWER      1U
#define CTRL2_BIT_PM25_RESET      2U
#define CTRL2_BIT_BT_PIO2         3U
#define CTRL2_BIT_BT_PIO3         4U
#define CTRL2_BIT_BT_PIO4         5U
#define CTRL2_BIT_AUDIO_MUTE      6U
#define CTRL2_BIT_FAN             7U
#define CTRL2_BIT_USB_SEL         8U
#define CTRL2_BIT_LORA_POWER      9U
#define CTRL2_BIT_LORA_M1         10U
#define CTRL2_BIT_LORA_M0         11U
#define CTRL2_BIT_ALARM_LIGHT     13U
#define CTRL2_BIT_SCREEN          14U

#define CTRL2_DEFAULT_IO          0x00000186U  /* bit1/2/7/8；bit9 LoRa 默认关 */
#define CTRL2_DEFAULT_NEIJI_CTRL  ((1UL << CTRL2_BIT_ALARM_LIGHT) | \
                                   (1UL << CTRL2_BIT_SCREEN))
#define CTRL2_DEFAULT             (CTRL2_DEFAULT_IO | CTRL2_DEFAULT_NEIJI_CTRL)

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

/** bit9=1：LoRa 上电且协议桥接开；bit9=0：断电且不走 LoRa */
int Config_LoraEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_FLASH_H */

