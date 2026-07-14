#include "lora_boot.h"

#include "config_flash.h"
#include "main.h"
#include "usart.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

static void Lora_YieldDelay(uint32_t ms)
{
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    osDelay(ms);
  }
  else
  {
    HAL_Delay(ms);
  }
}

#define LORA_CFG_RSP_LEN  6U
#define LORA_CHAN_MHZ(ch) (410U + (uint32_t)(ch))
/* 1=上电打印 LoRa 参数到 USART1 */
#define LORA_BOOT_LOG       0
#ifndef LORA_AUX_TIMEOUT_MS
#define LORA_AUX_TIMEOUT_MS 500U
#endif

uint8_t Lora_WaitAuxReady(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  while (HAL_GPIO_ReadPin(LORA_AUX_GPIO_Port, LORA_AUX_Pin) == GPIO_PIN_RESET)
  {
    if ((HAL_GetTick() - start) >= timeout_ms)
    {
      return 0U;
    }
    /* 调度运行后用 osDelay，避免 HAL_Delay 占死协议任务不让出 */
    Lora_YieldDelay(1U);
  }
  /* 手册建议 AUX 拉高后再略等再发 */
  Lora_YieldDelay(2U);
  return 1U;
}

HAL_StatusTypeDef Lora_Usart2Tx(const uint8_t *buf, uint16_t len, uint32_t timeout)
{
  HAL_StatusTypeDef status;

  (void)Lora_WaitAuxReady(LORA_AUX_TIMEOUT_MS);
  status = USART2_Tx(buf, len, timeout);
  (void)Lora_WaitAuxReady(LORA_AUX_TIMEOUT_MS);
  return status;
}

static void lora_cfg_irq_disable(void)
{
  __HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);
  HAL_NVIC_DisableIRQ(USART2_IRQn);
}

static void lora_hw_drain(void)
{
  while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != 0U)
  {
    (void)(huart2.Instance->DR & 0xFFU);
  }
}

static void lora_set_config_mode(uint8_t config_mode)
{
  HAL_GPIO_WritePin(LORA_M0_GPIO_Port, LORA_M0_Pin,
                    config_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LORA_M1_GPIO_Port, LORA_M1_Pin,
                    config_mode ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint16_t lora_poll_rx(uint8_t *buf, uint16_t cap, uint32_t timeout_ms)
{
  uint16_t got = 0U;
  uint32_t start = HAL_GetTick();

  while ((got < cap) && ((HAL_GetTick() - start) < timeout_ms))
  {
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != 0U)
    {
      buf[got++] = (uint8_t)(huart2.Instance->DR & 0xFFU);
    }
  }

  while ((got < cap) && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != 0U))
  {
    buf[got++] = (uint8_t)(huart2.Instance->DR & 0xFFU);
  }

  return got;
}

static uint8_t lora_parse_cfg_rsp(const uint8_t *rx, uint16_t rx_len,
                                  uint8_t *addr_h, uint8_t *addr_l,
                                  uint8_t *sped, uint8_t *chan, uint8_t *option)
{
  const uint8_t *p;

  if ((rx == NULL) || (addr_h == NULL) || (addr_l == NULL) ||
      (sped == NULL) || (chan == NULL) || (option == NULL))
  {
    return 0U;
  }

  if ((rx_len >= LORA_CFG_RSP_LEN) && ((rx[0] == 0xC0) || (rx[0] == 0xC2)))
  {
    p = &rx[1];
  }
  else if ((rx_len >= 5U) && (rx[0] != 0xC0) && (rx[0] != 0xC1) && (rx[0] != 0xC2))
  {
    p = &rx[0];
  }
  else
  {
    return 0U;
  }

  *addr_h = p[0];
  *addr_l = p[1];
  *sped = p[2];
  *chan = p[3];
  *option = p[4];
  return 1U;
}

static uint8_t lora_cfg_matches_target(uint8_t addr_h, uint8_t addr_l,
                                       uint8_t sped, uint8_t chan, uint8_t option)
{
  return (addr_h == LORA_TARGET_ADDR_H) &&
         (addr_l == LORA_TARGET_ADDR_L) &&
         (sped == LORA_TARGET_SPED) &&
         (chan == LORA_TARGET_CHAN) &&
         (option == LORA_TARGET_OPTION);
}

static uint8_t lora_cfg_read(uint8_t *addr_h, uint8_t *addr_l,
                             uint8_t *sped, uint8_t *chan, uint8_t *option)
{
  static const uint8_t read_cmd[] = {0xC1, 0xC1, 0xC1};
  uint8_t rx[8];
  uint16_t rx_len = 0U;
  uint8_t attempt;
  uint8_t ok = 0U;

  for (attempt = 0U; (attempt < 3U) && (ok == 0U); attempt++)
  {
    lora_set_config_mode(1U);
    HAL_Delay(100U);
    lora_hw_drain();

    if (HAL_UART_Transmit(&huart2, (uint8_t *)read_cmd, 3U, 200U) != HAL_OK)
    {
      break;
    }

    rx_len = lora_poll_rx(rx, sizeof(rx), 300U);
    ok = lora_parse_cfg_rsp(rx, rx_len, addr_h, addr_l, sped, chan, option);
  }

  return ok;
}

static uint8_t lora_cfg_write_target(void)
{
  static const uint8_t write_cmd[] = {
      0xC0,
      LORA_TARGET_ADDR_H,
      LORA_TARGET_ADDR_L,
      LORA_TARGET_SPED,
      LORA_TARGET_CHAN,
      LORA_TARGET_OPTION
  };

  lora_set_config_mode(1U);
  HAL_Delay(100U);
  lora_hw_drain();

  if (HAL_UART_Transmit(&huart2, (uint8_t *)write_cmd, sizeof(write_cmd), 200U) != HAL_OK)
  {
    return 0U;
  }

  (void)Lora_WaitAuxReady(LORA_AUX_TIMEOUT_MS);
  return 1U;
}

uint8_t Lora_BootApplyConfig(void)
{
  static const char *air_rate_str[] = {
      "300bps", "1.2kbps", "2.4kbps", "4.8kbps", "9.6kbps", "19.2kbps"
  };
  static const char *tx_power_str[] = {
      "20dBm", "17dBm", "14dBm", "10dBm"
  };

  uint8_t addr_h = 0U;
  uint8_t addr_l = 0U;
  uint8_t sped = 0U;
  uint8_t chan = 0U;
  uint8_t option = 0U;
  uint8_t ok = 0U;

  lora_cfg_irq_disable();
  lora_hw_drain();

  if (lora_cfg_read(&addr_h, &addr_l, &sped, &chan, &option) == 0U)
  {
    (void)lora_cfg_write_target();
    ok = lora_cfg_read(&addr_h, &addr_l, &sped, &chan, &option);
  }
  else if (lora_cfg_matches_target(addr_h, addr_l, sped, chan, option) == 0U)
  {
    (void)lora_cfg_write_target();
    ok = lora_cfg_read(&addr_h, &addr_l, &sped, &chan, &option);
  }
  else
  {
    ok = 1U;
  }

  lora_set_config_mode(0U);
  HAL_Delay(50U);
  USART2_ReinitBaud(LORA_USART_BAUD_RUN);
  Config_ApplyIoOutputs();
  USART2_Rx_Start();

  if (ok == 0U)
  {
#if LORA_BOOT_LOG
    printf("[LORA] ready 115200 mode0 (config fail)\r\n");
#endif
    return 0U;
  }

  {
    uint16_t addr = (uint16_t)(((uint16_t)addr_h << 8) | addr_l);
    uint8_t air_idx = sped & 0x07U;
    uint8_t pwr_idx = option & 0x03U;

#if LORA_BOOT_LOG
    printf("[LORA] ready 115200 mode0 addr=%u chan=%u(%luMHz) "
           "sped=0x%02X opt=0x%02X air=%s pwr=%s\r\n",
           (unsigned)addr,
           (unsigned)chan,
           (unsigned long)LORA_CHAN_MHZ(chan),
           (unsigned)sped,
           (unsigned)option,
           (air_idx <= 5U) ? air_rate_str[air_idx] : "?",
           (pwr_idx <= 3U) ? tx_power_str[pwr_idx] : "?");
#endif
  }

  return 1U;
}
