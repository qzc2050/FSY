/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "cmsis_os.h"
/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart3;

/* USER CODE BEGIN Private defines */
#define USART1_RX_BUF_SIZE  256
#define USART2_RX_BUF_SIZE  512
/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);

/* USER CODE BEGIN Prototypes */
void USART1_Rx_Start(void);
uint16_t USART1_Rx_GetCount(void);
uint16_t USART1_Rx_Read(uint8_t *buf, uint16_t len);
void USART1_Rx_PushByte(uint8_t byte);
void USART1_TxInit(void);
HAL_StatusTypeDef USART1_Tx(const uint8_t *buf, uint16_t len, uint32_t timeout);
typedef struct
{
  volatile uint32_t tx_ok;
  volatile uint32_t tx_busy;
  volatile uint32_t tx_timeout;
  volatile uint32_t tx_error;
  volatile uint32_t mutex_timeout;
  volatile uint32_t recover_count;
  volatile uint32_t consecutive_failures;
  volatile uint32_t last_ok_tick;
} USART1_Diag_t;
extern USART1_Diag_t g_usart1_diag;
/** 仅在实际连续发送失败时恢复 HAL；合法静默/OTA 空闲不复位 */
void USART1_HealthService(void);
void USART2_Rx_Start(void);
uint16_t USART2_Rx_GetCount(void);
uint16_t USART2_Rx_Read(uint8_t *buf, uint16_t len);
void USART2_Rx_PushByte(uint8_t byte);
void USART2_TxInit(void);
HAL_StatusTypeDef USART2_Tx(const uint8_t *buf, uint16_t len, uint32_t timeout);
void USART2_ReinitBaud(uint32_t baud);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

