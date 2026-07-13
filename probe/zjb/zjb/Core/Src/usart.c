/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
/* Includes ------------------------------------------------------------------*/
#include "usart.h"
#include "cmsis_os.h"

/* USER CODE BEGIN 0 */
static uint8_t usart1_rx_buf[USART1_RX_BUF_SIZE];
static volatile uint16_t usart1_rx_head;
static volatile uint16_t usart1_rx_tail;
static osMutexId_t s_usart1_tx_mutex;

void USART1_Rx_PushByte(uint8_t byte)
{
  uint16_t next = (usart1_rx_head + 1U) % USART1_RX_BUF_SIZE;
  if (next != usart1_rx_tail)
  {
    usart1_rx_buf[usart1_rx_head] = byte;
    usart1_rx_head = next;
  }
}

uint16_t USART1_Rx_GetCount(void)
{
  uint16_t tail = usart1_rx_tail;
  uint16_t head = usart1_rx_head;
  if (head >= tail)
    return (uint16_t)(head - tail);
  return (uint16_t)(USART1_RX_BUF_SIZE - tail + head);
}

uint16_t USART1_Rx_Read(uint8_t *buf, uint16_t len)
{
  uint16_t n = 0;
  while (n < len && usart1_rx_tail != usart1_rx_head)
  {
    buf[n++] = usart1_rx_buf[usart1_rx_tail];
    usart1_rx_tail = (usart1_rx_tail + 1U) % USART1_RX_BUF_SIZE;
  }
  return n;
}

void USART1_Rx_Start(void)
{
  usart1_rx_head = 0;
  usart1_rx_tail = 0;
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void USART1_TxInit(void)
{
  static const osMutexAttr_t attr = { .name = "usart1Tx" };
  s_usart1_tx_mutex = osMutexNew(&attr);
}

HAL_StatusTypeDef USART1_Tx(const uint8_t *buf, uint16_t len, uint32_t timeout)
{
  HAL_StatusTypeDef status;

  if ((buf == NULL) || (len == 0U))
  {
    return HAL_ERROR;
  }

  if (s_usart1_tx_mutex != NULL)
  {
    (void)osMutexAcquire(s_usart1_tx_mutex, osWaitForever);
  }

  status = HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, timeout);

  if (s_usart1_tx_mutex != NULL)
  {
    (void)osMutexRelease(s_usart1_tx_mutex);
  }

  return status;
}

static uint8_t usart2_rx_buf[USART2_RX_BUF_SIZE];
static volatile uint16_t usart2_rx_head;
static volatile uint16_t usart2_rx_tail;
static osMutexId_t s_usart2_tx_mutex;

void USART2_Rx_PushByte(uint8_t byte)
{
  uint16_t next = (usart2_rx_head + 1U) % USART2_RX_BUF_SIZE;
  if (next != usart2_rx_tail)
  {
    usart2_rx_buf[usart2_rx_head] = byte;
    usart2_rx_head = next;
  }
}

uint16_t USART2_Rx_GetCount(void)
{
  uint16_t tail = usart2_rx_tail;
  uint16_t head = usart2_rx_head;
  if (head >= tail)
    return (uint16_t)(head - tail);
  return (uint16_t)(USART2_RX_BUF_SIZE - tail + head);
}

uint16_t USART2_Rx_Read(uint8_t *buf, uint16_t len)
{
  uint16_t n = 0;
  while (n < len && usart2_rx_tail != usart2_rx_head)
  {
    buf[n++] = usart2_rx_buf[usart2_rx_tail];
    usart2_rx_tail = (usart2_rx_tail + 1U) % USART2_RX_BUF_SIZE;
  }
  return n;
}

void USART2_Rx_Start(void)
{
  usart2_rx_head = 0;
  usart2_rx_tail = 0;
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
  HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_TxInit(void)
{
  static const osMutexAttr_t attr = { .name = "usart2Tx" };
  s_usart2_tx_mutex = osMutexNew(&attr);
}

HAL_StatusTypeDef USART2_Tx(const uint8_t *buf, uint16_t len, uint32_t timeout)
{
  HAL_StatusTypeDef status;

  if ((buf == NULL) || (len == 0U))
  {
    return HAL_ERROR;
  }

  if (s_usart2_tx_mutex != NULL)
  {
    (void)osMutexAcquire(s_usart2_tx_mutex, osWaitForever);
  }

  status = HAL_UART_Transmit(&huart2, (uint8_t *)buf, len, timeout);

  if (s_usart2_tx_mutex != NULL)
  {
    (void)osMutexRelease(s_usart2_tx_mutex);
  }

  return status;
}

void USART2_ReinitBaud(uint32_t baud)
{
  __HAL_UART_DISABLE_IT(&huart2, UART_IT_RXNE);
  HAL_NVIC_DisableIRQ(USART2_IRQn);

  if (HAL_UART_DeInit(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

  huart2.Init.BaudRate = baud;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}
/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  USART1_Rx_Start();
  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  HAL_Delay(200U);
  USART2_Rx_Start();
  /* USER CODE END USART2_Init 2 */

}
/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = LORA_USART2_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LORA_USART2_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LORA_USART2_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LORA_USART2_RX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN USART2_MspInit 1 */

  /* USER CODE END USART2_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */
    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = PM25_USART3_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PM25_USART3_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PM25_USART3_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(PM25_USART3_RX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, LORA_USART2_TX_Pin|LORA_USART2_RX_Pin);

  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB10     ------> USART3_TX
    PB11     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOB, PM25_USART3_TX_Pin|PM25_USART3_RX_Pin);

  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
