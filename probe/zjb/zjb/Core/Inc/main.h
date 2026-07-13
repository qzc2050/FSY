/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LORA_USART2_TX_Pin GPIO_PIN_2
#define LORA_USART2_TX_GPIO_Port GPIOA
#define LORA_USART2_RX_Pin GPIO_PIN_3
#define LORA_USART2_RX_GPIO_Port GPIOA
#define LORA_POWER_EN_Pin GPIO_PIN_4
#define LORA_POWER_EN_GPIO_Port GPIOA
#define LORA_M0_Pin GPIO_PIN_5
#define LORA_M0_GPIO_Port GPIOA
#define LORA_M1_Pin GPIO_PIN_6
#define LORA_M1_GPIO_Port GPIOA
#define DOOR_SW_Pin GPIO_PIN_7
#define DOOR_SW_GPIO_Port GPIOA
#define PM2_5_POWER_EN_Pin GPIO_PIN_0
#define PM2_5_POWER_EN_GPIO_Port GPIOB
#define PM2_5_RESET_Pin GPIO_PIN_1
#define PM2_5_RESET_GPIO_Port GPIOB
#define PM25_USART3_TX_Pin GPIO_PIN_10
#define PM25_USART3_TX_GPIO_Port GPIOB
#define PM25_USART3_RX_Pin GPIO_PIN_11
#define PM25_USART3_RX_GPIO_Port GPIOB
/** E32 AUX：飞线至 PB12，空闲=高，忙/收发中=低 */
#define LORA_AUX_Pin GPIO_PIN_12
#define LORA_AUX_GPIO_Port GPIOB
#define USB_SEL_Pin GPIO_PIN_15
#define USB_SEL_GPIO_Port GPIOB
#define BT_PIO2_Pin GPIO_PIN_3
#define BT_PIO2_GPIO_Port GPIOB
#define BT_PIO3_Pin GPIO_PIN_4
#define BT_PIO3_GPIO_Port GPIOB
#define BT_PIO4_Pin GPIO_PIN_5
#define BT_PIO4_GPIO_Port GPIOB
#define AUDIO_MUTE_Pin GPIO_PIN_15
#define AUDIO_MUTE_GPIO_Port GPIOA
#define I2C1_SCL_SENSOR_Pin GPIO_PIN_6
#define I2C1_SCL_SENSOR_GPIO_Port GPIOB
#define I2C1_SDA_SENSOR_Pin GPIO_PIN_7
#define I2C1_SDA_SENSOR_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
