/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#define HLK_WDT_RST_Pin GPIO_PIN_8
#define HLK_WDT_RST_GPIO_Port GPIOI
#define HV_EN_Pin GPIO_PIN_3
#define HV_EN_GPIO_Port GPIOH
#define LCD_RST_Pin GPIO_PIN_4
#define LCD_RST_GPIO_Port GPIOH
#define LCD_CS_Pin GPIO_PIN_5
#define LCD_CS_GPIO_Port GPIOH
#define KEY_PRESS_Pin GPIO_PIN_4
#define KEY_PRESS_GPIO_Port GPIOA
#define KEY_UP_DOWN_Pin GPIO_PIN_4
#define KEY_UP_DOWN_GPIO_Port GPIOC
#define KEY_LEFT_RIGHT_Pin GPIO_PIN_5
#define KEY_LEFT_RIGHT_GPIO_Port GPIOC
#define PM25_USART_RX_Pin GPIO_PIN_11
#define PM25_USART_RX_GPIO_Port GPIOB
#define LCD_BACKLIGHT_Pin GPIO_PIN_6
#define LCD_BACKLIGHT_GPIO_Port GPIOH
#define KEY_SET_Pin GPIO_PIN_7
#define KEY_SET_GPIO_Port GPIOH
#define BEEP_PWM_Pin GPIO_PIN_9
#define BEEP_PWM_GPIO_Port GPIOH
#define PM25_RST_Pin GPIO_PIN_10
#define PM25_RST_GPIO_Port GPIOH
#define WS2812B_PWM_Pin GPIO_PIN_12
#define WS2812B_PWM_GPIO_Port GPIOD
#define PM25_USART_TX_Pin GPIO_PIN_10
#define PM25_USART_TX_GPIO_Port GPIOC
#define LORA_TX_Pin GPIO_PIN_12
#define LORA_TX_GPIO_Port GPIOC
#define LORA_RX_Pin GPIO_PIN_2
#define LORA_RX_GPIO_Port GPIOD
#define LORA_AUX_Pin GPIO_PIN_3
#define LORA_AUX_GPIO_Port GPIOD
#define LORA_M1_Pin GPIO_PIN_4
#define LORA_M1_GPIO_Port GPIOD
#define LORA_M0_Pin GPIO_PIN_5
#define LORA_M0_GPIO_Port GPIOD
#define W5500_MOSI_Pin GPIO_PIN_7
#define W5500_MOSI_GPIO_Port GPIOD
#define W5500_MISO_Pin GPIO_PIN_9
#define W5500_MISO_GPIO_Port GPIOG
#define W5500_CS_Pin GPIO_PIN_10
#define W5500_CS_GPIO_Port GPIOG
#define W5500_SCK_Pin GPIO_PIN_11
#define W5500_SCK_GPIO_Port GPIOG
#define W5500_LINK_Pin GPIO_PIN_3
#define W5500_LINK_GPIO_Port GPIOB
#define W5500_DUP_Pin GPIO_PIN_4
#define W5500_DUP_GPIO_Port GPIOB
#define W5500_ACT_Pin GPIO_PIN_5
#define W5500_ACT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
// zero_init初始化内存
// zero_init不可省略，否则可能导致系统崩溃
#define DEV_MALLOC_EXSRAM       __attribute__((section(".RAM_EX_SDRAM"), zero_init))







/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
