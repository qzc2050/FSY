#include "protec_protocol.h"
#include "usart.h"
#include "pm25.h"
#include "aht20.h"
#include "bmp280.h"
#include "ens160.h"
#include "fan.h"
#include "main.h"

#define PROTEC_FUNC_ACTIVE_MULTI   0x23U

#define SW_VERSION_STR "V1.1.6.20260721G"

SystemStatus_t g_system_status;
const char g_sw_version[20] = SW_VERSION_STR;

/* 起始寄存器地址：1，对应 g_system_status 的第一个字段 */
#define PROTEC_START_REG_L   1U
#define PROTEC_START_REG_H   0x00U

/* 目前帧内发送的 uint32 寄存器个数 */
#define PROTEC_REG_COUNT     11U
#define PROTEC_IO_STATUS_MASK 0x00000FFFU

static uint8_t s_protec_tx_buf[3U + 2U + PROTEC_REG_COUNT * 4U + 2U];
static uint8_t s_bad_upload_count;

static uint8_t Protec_PayloadLooksValid(void)
{
  if ((g_system_status.io_status & ~PROTEC_IO_STATUS_MASK) != 0U)
  {
    return 0U;
  }

  if ((g_system_status.reserve1 != 0U) ||
      (g_system_status.reserve2 != 0U) ||
      (g_system_status.reserve3 != 0U))
  {
    return 0U;
  }

  /* 正常上报后，传感器全 0 且 alarm 也为 0 属于异常组合 */
  if ((g_system_status.alarm_bit1 == 0U) &&
      (g_system_status.temp == 0U) &&
      (g_system_status.press == 0U) &&
      (g_system_status.hum == 0U) &&
      (g_system_status.co2 == 0U) &&
      (g_system_status.pm2d5 == 0U))
  {
    return 0U;
  }

  return 1U;
}

static uint16_t Protec_CalcCrc(const uint8_t *buf, uint16_t len)
{
  uint16_t crc = 0xFFFFU;
  uint16_t i;
  uint8_t j;

  for (i = 0U; i < len; i++)
  {
    crc ^= (uint16_t)buf[i];
    for (j = 0U; j < 8U; j++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc >>= 1;
        crc ^= 0xA001U;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

void Protec_Init(void)
{
  g_system_status.dose_rate  = 0U;
  g_system_status.temp       = 0U;
  g_system_status.press      = 0U;
  g_system_status.hum        = 0U;
  g_system_status.co2        = 0U;
  g_system_status.pm2d5      = 0U;
  g_system_status.alarm_bit1 = 0U;
  g_system_status.io_status  = 0U;
  g_system_status.reserve1   = 0U;
  g_system_status.reserve2   = 0U;
  g_system_status.reserve3   = 0U;
  s_bad_upload_count         = 0U;
}

void Protec_SendRealtime(void)
{
  AHT20_Data_t aht;
  BMP280_Data_t bmp;
  ENS160_Data_t ens;
  PM25_Data_t pm25;
  uint32_t alarm = 0U;
  uint32_t io_status = 0U;
  GPIO_PinState door_state;
  uint8_t fan_state;

  uint8_t *buf = s_protec_tx_buf;
  uint16_t idx = 0U;
  uint16_t crc;
  uint32_t *p_val;
  uint8_t i;

  AHT20_GetData(&aht);
  BMP280_GetData(&bmp);
  ENS160_GetData(&ens);
  PM25_GetData(&pm25);

  /* 填充 g_system_status */
  g_system_status.dose_rate = 0U; /* 当前板无辐射传感器 */
  g_system_status.temp = (uint32_t)((int32_t)(aht.temperature_c * 10.0f));
  g_system_status.press = (uint32_t)bmp.pressure_pa;  /* Pa，精度留给上位机 */
  g_system_status.hum = (uint32_t)aht.humidity_rh;
  g_system_status.co2 = (uint32_t)ens.eco2;
  g_system_status.pm2d5 = (uint32_t)pm25.pm2_5 * 10U;

  if (aht.online == 0U)
  {
    alarm |= (1UL << 6);   /* 温度检测离线 */
    alarm |= (1UL << 14);  /* 湿度检测离线 */
  }
  if (bmp.online == 0U)
  {
    alarm |= (1UL << 10);  /* 气压检测离线 */
  }
  if (ens.online == 0U)
  {
    alarm |= (1UL << 18);  /* CO2 检测离线 */
  }
  if (pm25.online == 0U)
  {
    alarm |= (1UL << 22);  /* PM2.5 检测离线 */
  }
  g_system_status.alarm_bit1 = alarm;

  /*
   * io_status (reg15) uint32, bit0~bit11 有效，高位预留
   * bit0:  门状态                    0 打开  1 关闭
   * bit1:  PM2.5 电源                0 低    1 高 (PM2_5_POWER_EN)
   * bit2:  PM2.5 复位                0 低    1 高 (PM2_5_RESET)
   * bit3:  蓝牙暂停播放键状态(BT_PIO2)  0 低电平  1 高电平（低电平=按下）
   * bit4:  蓝牙音量-键状态(BT_PIO3)     0 低电平  1 高电平（低电平=按下）
   * bit5:  蓝牙音量+键状态(BT_PIO4)     0 低电平  1 高电平（低电平=按下）
   * bit6:  蓝牙静音键状态(AUDIO_MUTE)   0 低电平  1 高电平（高电平静音，低电平=按下）
   * bit7:  风扇开关                   0 关闭  1 开启
   * bit8:  USB 路由 (USB_SEL)        0 选 USB2，1 选 USB3
   * bit9:  LORA 电源                  0 低    1 高 (LORA_POWER_EN)
   * bit10: LORA 模式 M1               0 低    1 高
   * bit11: LORA 模式 M0               0 低    1 高
   */
  door_state = HAL_GPIO_ReadPin(DOOR_SW_GPIO_Port, DOOR_SW_Pin);
  if (door_state == GPIO_PIN_RESET)
  {
    io_status |= (1UL << 0);   /* 关闭 */
  }

  if ((PM2_5_POWER_EN_GPIO_Port->ODR & PM2_5_POWER_EN_Pin) != 0U)
  {
    io_status |= (1UL << 1);
  }
  if ((PM2_5_RESET_GPIO_Port->ODR & PM2_5_RESET_Pin) != 0U)
  {
    io_status |= (1UL << 2);
  }
  if (HAL_GPIO_ReadPin(BT_PIO2_GPIO_Port, BT_PIO2_Pin) == GPIO_PIN_SET)
  {
    io_status |= (1UL << 3);
  }
  if (HAL_GPIO_ReadPin(BT_PIO3_GPIO_Port, BT_PIO3_Pin) == GPIO_PIN_SET)
  {
    io_status |= (1UL << 4);
  }
  if (HAL_GPIO_ReadPin(BT_PIO4_GPIO_Port, BT_PIO4_Pin) == GPIO_PIN_SET)
  {
    io_status |= (1UL << 5);
  }
  if ((AUDIO_MUTE_GPIO_Port->ODR & AUDIO_MUTE_Pin) != 0U)
  {
    io_status |= (1UL << 6);
  }
  fan_state = Fan_Get();
  if (fan_state != 0U)
  {
    io_status |= (1UL << 7);
  }
  if ((USB_SEL_GPIO_Port->ODR & USB_SEL_Pin) != 0U)
  {
    io_status |= (1UL << 8);
  }
  if ((LORA_POWER_EN_GPIO_Port->ODR & LORA_POWER_EN_Pin) != 0U)
  {
    io_status |= (1UL << 9);
  }
  if ((LORA_M1_GPIO_Port->ODR & LORA_M1_Pin) != 0U)
  {
    io_status |= (1UL << 10);
  }
  if ((LORA_M0_GPIO_Port->ODR & LORA_M0_Pin) != 0U)
  {
    io_status |= (1UL << 11);
  }

  g_system_status.io_status = io_status;

  /* 预留字段先清零 */
  g_system_status.reserve1 = 0U;
  g_system_status.reserve2 = 0U;
  g_system_status.reserve3 = 0U;

  if (Protec_PayloadLooksValid() == 0U)
  {
    /*
     * 传感器离线/预热/异常零值不应导致整机反复复位。
     * 保留计数供 ST-Link 观察，并置各传感器离线报警后继续发送，
     * 让上位机仍能判断 ZJB 在线及查看 IO 状态。
     */
    if (s_bad_upload_count < 0xFFU)
    {
      s_bad_upload_count++;
    }
    g_system_status.alarm_bit1 |=
        (1UL << 6)  |  /* 温度离线 */
        (1UL << 10) |  /* 气压离线 */
        (1UL << 14) |  /* 湿度离线 */
        (1UL << 18) |  /* CO2 离线 */
        (1UL << 22);   /* PM2.5 离线 */
  }
  else
  {
    s_bad_upload_count = 0U;
  }

  /* 组一帧 0x23 帧 */
  buf[idx++] = (uint8_t)PROTEC_ADDR;
  buf[idx++] = PROTEC_FUNC_ACTIVE_MULTI;
  buf[idx++] = (uint8_t)(PROTEC_REG_COUNT * 4U); /* 字节数 */
  buf[idx++] = PROTEC_START_REG_L;
  buf[idx++] = PROTEC_START_REG_H;

  p_val = (uint32_t *)&g_system_status;
  for (i = 0U; i < PROTEC_REG_COUNT; i++)
  {
    uint32_t v = p_val[i];
    buf[idx++] = (uint8_t)(v & 0xFFU);
    buf[idx++] = (uint8_t)((v >> 8) & 0xFFU);
    buf[idx++] = (uint8_t)((v >> 16) & 0xFFU);
    buf[idx++] = (uint8_t)((v >> 24) & 0xFFU);
  }

  crc = Protec_CalcCrc(buf, idx);
  buf[idx++] = (uint8_t)(crc & 0xFFU);
  buf[idx++] = (uint8_t)((crc >> 8) & 0xFFU);

  (void)USART1_Tx(buf, idx, 100U);
}

