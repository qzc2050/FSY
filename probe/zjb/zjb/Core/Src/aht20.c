#include "aht20.h"
#include "i2c.h"
#include "main.h"

#define AHT20_I2C_ADDR         (0x38U << 1)

static AHT20_Data_t s_aht20;

static HAL_StatusTypeDef AHT20_WriteCmd(uint8_t cmd, const uint8_t *data, uint16_t len)
{
  uint8_t buf[3];

  if (len > 2U)
  {
    return HAL_ERROR;
  }

  buf[0] = cmd;
  if (len >= 1U)
  {
    buf[1] = data[0];
  }
  if (len == 2U)
  {
    buf[2] = data[1];
  }

  return HAL_I2C_Master_Transmit(&hi2c1, AHT20_I2C_ADDR, buf, (uint16_t)(1U + len), 50U);
}

void AHT20_Init(void)
{
  uint8_t data[2];

  s_aht20.temperature_c = 0.0f;
  s_aht20.humidity_rh = 0.0f;
  s_aht20.online = 0U;
  s_aht20.last_update_tick = 0U;

  data[0] = 0x08U;
  data[1] = 0x00U;
  (void)AHT20_WriteCmd(0xBEU, data, 2U);
  HAL_Delay(20U);
}

void AHT20_Update(void)
{
  uint8_t cmd_data[2];
  uint8_t buf[6];
  uint32_t raw_h;
  uint32_t raw_t;

  cmd_data[0] = 0x33U;
  cmd_data[1] = 0x00U;
  if (AHT20_WriteCmd(0xACU, cmd_data, 2U) != HAL_OK)
  {
    return;
  }

  HAL_Delay(80U);

  if (HAL_I2C_Master_Receive(&hi2c1, AHT20_I2C_ADDR, buf, sizeof(buf), 50U) != HAL_OK)
  {
    return;
  }

  raw_h = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
  raw_t = (((uint32_t)buf[3] & 0x0FU) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];

  s_aht20.humidity_rh = (float)raw_h * 100.0f / 1048576.0f;
  s_aht20.temperature_c = (float)raw_t * 200.0f / 1048576.0f - 50.0f;

  s_aht20.last_update_tick = HAL_GetTick();
  s_aht20.online = 1U;
}

void AHT20_GetData(AHT20_Data_t *out)
{
  uint32_t now;
  uint32_t diff;

  if (out == NULL)
  {
    return;
  }

  *out = s_aht20;

  now = HAL_GetTick();
  diff = now - s_aht20.last_update_tick;

  if ((s_aht20.last_update_tick == 0U) || (diff > 5000U))
  {
    out->online = 0U;
  }
  else
  {
    out->online = 1U;
  }
}

