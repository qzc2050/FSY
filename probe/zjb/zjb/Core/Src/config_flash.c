#include "config_flash.h"
#include "main.h"
#include "gpio.h"
#include "fan.h"

#include <string.h>

SystemConfig_t g_config;

static uint32_t Config_CalcCrc(const SystemConfig_t *cfg)
{
  /* 简单 CRC32 实现，标准多项式 0x04C11DB7 */
  uint32_t crc = 0xFFFFFFFFU;
  const uint8_t *p = (const uint8_t *)cfg;
  uint32_t len = sizeof(SystemConfig_t) - sizeof(uint32_t); /* 不含 crc 字段 */
  uint32_t i;
  uint8_t j;

  for (i = 0U; i < len; i++)
  {
    crc ^= ((uint32_t)p[i] << 24);
    for (j = 0U; j < 8U; j++)
    {
      if ((crc & 0x80000000U) != 0U)
      {
        crc = (crc << 1) ^ 0x04C11DB7U;
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

void Config_SetDefault(void)
{
  memset(&g_config, 0, sizeof(g_config));

  g_config.magic   = CONFIG_MAGIC;
  g_config.version = CONFIG_VERSION;

  /* 这里填一套简单默认值，你后续可以按需要调整 */
  g_config.temp_up    = 350;   /* 35.0℃ */
  g_config.temp_down  = 150;   /* 15.0℃ */
  g_config.hum_up     = 800;   /* 80% */
  g_config.hum_down   = 200;   /* 20% */
  g_config.co2_up     = 2000;  /* 2000ppm */
  g_config.co2_down   = 400;   /* 400ppm */
  g_config.pm25_up    = 750;   /* 75 ug/m3 *10 */
  g_config.pm25_down  = 0;

  g_config.address     = 0x00EFU;  /* 默认协议地址，实际可由上位机修改 */
  g_config.control_bit = 0x0000U;  /* 默认全部启用声/光/屏幕 */
  g_config.control_bit2 = 0x00000386U; /* 默认 IO：bit3/4/5 蓝牙 PIO2/3/4 低电平，bit8=1 选 USB3（调试） */ 

  /* 默认序列号填零 */
  memset(g_config.serialnum, 0, sizeof(g_config.serialnum));

  g_config.crc = Config_CalcCrc(&g_config);
}

static void Config_ReadFromFlash(SystemConfig_t *out)
{
  const SystemConfig_t *pflash = (const SystemConfig_t *)CONFIG_FLASH_ADDR;
  memcpy(out, pflash, sizeof(SystemConfig_t));
}

static void Config_WriteToFlash(const SystemConfig_t *cfg)
{
  HAL_FLASH_Unlock();

  /* 擦除最后一页 */
  FLASH_EraseInitTypeDef erase;
  uint32_t page_error = 0U;

  erase.TypeErase   = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = CONFIG_FLASH_ADDR;
  erase.NbPages     = 1U;

  (void)HAL_FLASHEx_Erase(&erase, &page_error);

  /* 按 32bit 写入 */
  const uint32_t *pdata = (const uint32_t *)cfg;
  uint32_t addr = CONFIG_FLASH_ADDR;
  uint32_t words = (sizeof(SystemConfig_t) + 3U) / 4U;
  uint32_t i;

  for (i = 0U; i < words; i++)
  {
    (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, pdata[i]);
    addr += 4U;
  }

  HAL_FLASH_Lock();
}

void Config_Load(void)
{
  SystemConfig_t tmp;

  Config_ReadFromFlash(&tmp);

  if ((tmp.magic == CONFIG_MAGIC) &&
      (tmp.version == CONFIG_VERSION))
  {
    uint32_t crc = Config_CalcCrc(&tmp);
    if (crc == tmp.crc)
    {
      memcpy(&g_config, &tmp, sizeof(SystemConfig_t));
      return;
    }
  }

  /* Flash 中无效，加载默认值并写回 */
  Config_SetDefault();
  Config_Save();
}

void Config_Save(void)
{
  g_config.magic   = CONFIG_MAGIC;
  g_config.version = CONFIG_VERSION;
  g_config.crc     = Config_CalcCrc(&g_config);

  Config_WriteToFlash(&g_config);
}

void Config_ApplyIoOutputs(void)
{
  uint32_t m = g_config.control_bit2;

  /* bit1: PM2.5 电源 */
  HAL_GPIO_WritePin(PM2_5_POWER_EN_GPIO_Port, PM2_5_POWER_EN_Pin,
                    (m & (1UL << 1)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit2: PM2.5 复位 */
  HAL_GPIO_WritePin(PM2_5_RESET_GPIO_Port, PM2_5_RESET_Pin,
                    (m & (1UL << 2)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit3: BT_PIO2 暂停键 */
  HAL_GPIO_WritePin(BT_PIO2_GPIO_Port, BT_PIO2_Pin,
                    (m & (1UL << 3)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit4: BT_PIO3 音量- */
  HAL_GPIO_WritePin(BT_PIO3_GPIO_Port, BT_PIO3_Pin,
                    (m & (1UL << 4)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit5: BT_PIO4 音量+ */
  HAL_GPIO_WritePin(BT_PIO4_GPIO_Port, BT_PIO4_Pin,
                    (m & (1UL << 5)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit6: AUDIO_MUTE 静音键 */
  HAL_GPIO_WritePin(AUDIO_MUTE_GPIO_Port, AUDIO_MUTE_Pin,
                    (m & (1UL << 6)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit7: 风扇开关 → 用 Fan_Set 控制，占空比在线程里生效 */
  if ((m & (1UL << 7)) != 0U)
  {
    Fan_Set(1U);
  }
  else
  {
    Fan_Set(0U);
  }

  /* bit8: USB 路由 — 0 USB2，1 USB3 */
  HAL_GPIO_WritePin(USB_SEL_GPIO_Port, USB_SEL_Pin,
                    (m & (1UL << 8)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit9: LORA 电源 */
  HAL_GPIO_WritePin(LORA_POWER_EN_GPIO_Port, LORA_POWER_EN_Pin,
                    (m & (1UL << 9)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit10: LORA M1 */
  HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin,
                    (m & (1UL << 10)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);

  /* bit11: LORA M0 */
  HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin,
                    (m & (1UL << 11)) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

