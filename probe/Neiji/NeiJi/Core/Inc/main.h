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
#include "stm32h7xx_hal.h"

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
#define HV_EN_Pin GPIO_PIN_3
#define HV_EN_GPIO_Port GPIOH
#define LCD_RST_Pin GPIO_PIN_4
#define LCD_RST_GPIO_Port GPIOH
#define LCD_CS_Pin GPIO_PIN_5
#define LCD_CS_GPIO_Port GPIOH
#define LCD_BACKLIGHT_Pin GPIO_PIN_6
#define LCD_BACKLIGHT_GPIO_Port GPIOH
#define W5500_RST_Pin GPIO_PIN_6
#define W5500_RST_GPIO_Port GPIOC
#define W5500_CS_Pin GPIO_PIN_10
#define W5500_CS_GPIO_Port GPIOG
#define W5500_DUP_Pin GPIO_PIN_4
#define W5500_DUP_GPIO_Port GPIOB
#define W5500_ACT_Pin GPIO_PIN_5
#define W5500_ACT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define DEV_MALLOC_EXSRAM  __attribute__((section(".RAM_EX_SDRAM"), zero_init))

#define KEY_PRESS_Pin GPIO_PIN_4
#define KEY_PRESS_GPIO_Port GPIOA
#define KEY_UP_DOWN_Pin GPIO_PIN_4
#define KEY_UP_DOWN_GPIO_Port GPIOC
#define KEY_LEFT_RIGHT_Pin GPIO_PIN_5
#define KEY_LEFT_RIGHT_GPIO_Port GPIOC
#define KEY_SET_Pin GPIO_PIN_7
#define KEY_SET_GPIO_Port GPIOH
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
