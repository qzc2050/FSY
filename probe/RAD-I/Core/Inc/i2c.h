/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.h
  * @brief   This file contains all the function prototypes for
  *          the i2c.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern I2C_HandleTypeDef hi2c1;

extern I2C_HandleTypeDef hi2c4;

/* USER CODE BEGIN Private defines */
#define I2C_BUS_TIMEOUT_MS       50U
#define I2C_BUS_INIT_TIMEOUT_MS  100U
/* USER CODE END Private defines */

void MX_I2C1_Init(void);
void MX_I2C4_Init(void);

/* USER CODE BEGIN Prototypes */
void I2C_BusMutex_Init(void);

HAL_StatusTypeDef I2C4_Mem_Read(uint16_t DevAddress, uint16_t MemAddress,
                                uint8_t *pData, uint16_t Size, uint32_t TimeoutMs);
HAL_StatusTypeDef I2C4_Mem_Write(uint16_t DevAddress, uint16_t MemAddress,
                                 uint8_t *pData, uint16_t Size, uint32_t TimeoutMs);
HAL_StatusTypeDef I2C4_Master_Transmit(uint16_t DevAddress, uint8_t *pData,
                                       uint16_t Size, uint32_t TimeoutMs);
HAL_StatusTypeDef I2C4_Master_Receive(uint16_t DevAddress, uint8_t *pData,
                                      uint16_t Size, uint32_t TimeoutMs);
HAL_StatusTypeDef I2C1_Master_Transmit(uint16_t DevAddress, uint8_t *pData,
                                       uint16_t Size, uint32_t TimeoutMs);
HAL_StatusTypeDef I2C1_Master_Receive(uint16_t DevAddress, uint8_t *pData,
                                      uint16_t Size, uint32_t TimeoutMs);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */

