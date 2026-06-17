/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_i2c1_mutex;
static SemaphoreHandle_t s_i2c4_mutex;

static SemaphoreHandle_t i2c_bus_mutex_get(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL) {
        return NULL;
    }
    if (hi2c->Instance == I2C1) {
        return s_i2c1_mutex;
    }
    if (hi2c->Instance == I2C4) {
        return s_i2c4_mutex;
    }
    return NULL;
}

static void i2c_bus_lock(I2C_HandleTypeDef *hi2c)
{
    SemaphoreHandle_t mux = i2c_bus_mutex_get(hi2c);
    if (mux != NULL) {
        (void)xSemaphoreTake(mux, portMAX_DELAY);
    }
}

static void i2c_bus_unlock(I2C_HandleTypeDef *hi2c)
{
    SemaphoreHandle_t mux = i2c_bus_mutex_get(hi2c);
    if (mux != NULL) {
        (void)xSemaphoreGive(mux);
    }
}

void I2C_BusMutex_Init(void)
{
    if (s_i2c1_mutex == NULL) {
        s_i2c1_mutex = xSemaphoreCreateMutex();
    }
    if (s_i2c4_mutex == NULL) {
        s_i2c4_mutex = xSemaphoreCreateMutex();
    }
}

HAL_StatusTypeDef I2C4_Mem_Read(uint16_t DevAddress, uint16_t MemAddress,
                                uint8_t *pData, uint16_t Size, uint32_t TimeoutMs)
{
    HAL_StatusTypeDef status;

    i2c_bus_lock(&hi2c4);
    status = HAL_I2C_Mem_Read(&hi2c4, DevAddress, MemAddress,
                              I2C_MEMADD_SIZE_8BIT, pData, Size, TimeoutMs);
    i2c_bus_unlock(&hi2c4);
    return status;
}

HAL_StatusTypeDef I2C4_Mem_Write(uint16_t DevAddress, uint16_t MemAddress,
                                 uint8_t *pData, uint16_t Size, uint32_t TimeoutMs)
{
    HAL_StatusTypeDef status;

    i2c_bus_lock(&hi2c4);
    status = HAL_I2C_Mem_Write(&hi2c4, DevAddress, MemAddress,
                               I2C_MEMADD_SIZE_8BIT, pData, Size, TimeoutMs);
    i2c_bus_unlock(&hi2c4);
    return status;
}

HAL_StatusTypeDef I2C4_Master_Transmit(uint16_t DevAddress, uint8_t *pData,
                                       uint16_t Size, uint32_t TimeoutMs)
{
    HAL_StatusTypeDef status;

    i2c_bus_lock(&hi2c4);
    status = HAL_I2C_Master_Transmit(&hi2c4, DevAddress, pData, Size, TimeoutMs);
    i2c_bus_unlock(&hi2c4);
    return status;
}

HAL_StatusTypeDef I2C4_Master_Receive(uint16_t DevAddress, uint8_t *pData,
                                      uint16_t Size, uint32_t TimeoutMs)
{
    HAL_StatusTypeDef status;

    i2c_bus_lock(&hi2c4);
    status = HAL_I2C_Master_Receive(&hi2c4, DevAddress, pData, Size, TimeoutMs);
    i2c_bus_unlock(&hi2c4);
    return status;
}

HAL_StatusTypeDef I2C1_Master_Transmit(uint16_t DevAddress, uint8_t *pData,
                                       uint16_t Size, uint32_t TimeoutMs)
{
    HAL_StatusTypeDef status;

    i2c_bus_lock(&hi2c1);
    status = HAL_I2C_Master_Transmit(&hi2c1, DevAddress, pData, Size, TimeoutMs);
    i2c_bus_unlock(&hi2c1);
    return status;
}

HAL_StatusTypeDef I2C1_Master_Receive(uint16_t DevAddress, uint8_t *pData,
                                      uint16_t Size, uint32_t TimeoutMs)
{
    HAL_StatusTypeDef status;

    i2c_bus_lock(&hi2c1);
    status = HAL_I2C_Master_Receive(&hi2c1, DevAddress, pData, Size, TimeoutMs);
    i2c_bus_unlock(&hi2c1);
    return status;
}
/* USER CODE END 0 */

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c4;

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00F03FF5;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}
/* I2C4 init function */
void MX_I2C4_Init(void)
{

  /* USER CODE BEGIN I2C4_Init 0 */

  /* USER CODE END I2C4_Init 0 */

  /* USER CODE BEGIN I2C4_Init 1 */

  /* USER CODE END I2C4_Init 1 */
  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x10801FC1;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C4_Init 2 */

  /* USER CODE END I2C4_Init 2 */

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInitStruct.I2c123ClockSelection = RCC_I2C123CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2C1 clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
  }
  else if(i2cHandle->Instance==I2C4)
  {
  /* USER CODE BEGIN I2C4_MspInit 0 */

  /* USER CODE END I2C4_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
    PeriphClkInitStruct.I2c4ClockSelection = RCC_I2C4CLKSOURCE_D3PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOH_CLK_ENABLE();
    /**I2C4 GPIO Configuration
    PH11     ------> I2C4_SCL
    PH12     ------> I2C4_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    /* I2C4 clock enable */
    __HAL_RCC_I2C4_CLK_ENABLE();
  /* USER CODE BEGIN I2C4_MspInit 1 */

  /* USER CODE END I2C4_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{

  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
  }
  else if(i2cHandle->Instance==I2C4)
  {
  /* USER CODE BEGIN I2C4_MspDeInit 0 */

  /* USER CODE END I2C4_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C4_CLK_DISABLE();

    /**I2C4 GPIO Configuration
    PH11     ------> I2C4_SCL
    PH12     ------> I2C4_SDA
    */
    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_11);

    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_12);

  /* USER CODE BEGIN I2C4_MspDeInit 1 */

  /* USER CODE END I2C4_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
