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
/* LVGL/显示探针版本：每次改探针或 flush 逻辑后 +1，便于串口日志对版 */
#define NEIJI_DIAG_BUILD  16U   /* 16=UDP 组播诊断日志(tx ok/fail) */
#ifndef NEIJI_BEEP_PROBE
#define NEIJI_BEEP_PROBE  0U   /* 1=上电串口蜂鸣探针；硬件已确认后保持 0 */
#endif
#ifndef NEIJI_BEEP_GPIO_HIGH_TEST
#define NEIJI_BEEP_GPIO_HIGH_TEST  0U   /* 1=PH9 常驻 GPIO 高电平硬件测试；测完改回 0 */
#endif
#ifndef NEIJI_UI_LIVE_REFRESH
#define NEIJI_UI_LIVE_REFRESH  1U   /* 1=主界面每秒刷新传感器/剂量/时间 */
#endif
#ifndef NEIJI_UI_RADIATION_SPIN
#define NEIJI_UI_RADIATION_SPIN  1   /* 1=辐射图标转轮动画 */
#endif
#ifndef NEIJI_WS2812_ENABLE
#define NEIJI_WS2812_ENABLE  1   /* 1=WS2812 灯带任务 */
#endif
#ifndef NEIJI_LTDC_FB_NOCACHE
#define NEIJI_LTDC_FB_NOCACHE  1   /* 1=LTDC 帧缓冲 MPU 非 cache */
#endif
#ifndef NEIJI_LTDC_DIAG
#define NEIJI_LTDC_DIAG  0   /* 1=周期串口 [ltdc] 探针 */
#endif
#ifndef NEIJI_DISP_FLUSH_PAD
#define NEIJI_DISP_FLUSH_PAD  2U   /* DMA2D 写帧缓冲后物理行 clean 扩边(像素) */
#endif
#ifndef NEIJI_DISP_VSYNC_FLUSH
#define NEIJI_DISP_VSYNC_FLUSH  0   /* 0=默认；1=每块 flush 等 VSYNC，动画多时极慢且传感器易判离线 */
#endif
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
#define BEEP_PWM_Pin GPIO_PIN_9
#define BEEP_PWM_GPIO_Port GPIOH
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
