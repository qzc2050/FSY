/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#include "rtc.h"

/* USER CODE BEGIN 0 */

//void RTC_AlarmStart(void);
#pragma pack(1)
typedef struct rrttcc
{
    uint8_t Hours;
    uint8_t Minutes;
    uint8_t Seconds;
    uint8_t WeekDay;
    uint8_t Month;
    uint8_t Day;
    uint8_t Year;
} rtc_time_t;
#pragma pack()

void RTC_GetTime(rtc_time_t* time)
{
    HAL_RTC_GetTime(&hrtc, (RTC_TimeTypeDef *)&time->Hours,   RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, (RTC_DateTypeDef *)&time->WeekDay, RTC_FORMAT_BIN);
}

void RTC_SetTime(rtc_time_t* time)
{
    HAL_RTC_SetTime(&hrtc, (RTC_TimeTypeDef *)&time->Hours,   RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, (RTC_DateTypeDef *)&time->WeekDay, RTC_FORMAT_BIN);
}


//void RTC_AlarmStart(void)
//{
//    RTC_AlarmTypeDef sAlarm = {0};
//    rtc_time_t tim = {0};

//    // 获取当前时间
//    RTC_GetTime(&tim);

//    sAlarm.AlarmTime.Hours = tim.Hours;
//    sAlarm.AlarmTime.Minutes = tim.Minutes;
//    sAlarm.AlarmTime.Seconds = tim.Seconds + 3;  /* 设置下次闹钟提醒时间是当前时间的3s之后 */
//    sAlarm.Alarm = RTC_ALARM_A;

//    // 启动闹钟中断事件
//    HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN);
//}
/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 0x24;
  hrtc.Init.SynchPrediv = 0;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 500, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);   //关闭RTC超时功能
  /* USER CODE END RTC_Init 2 */

}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */
    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();

    /* RTC interrupt Init */
    HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();

    /* RTC interrupt Deinit */
    HAL_NVIC_DisableIRQ(RTC_IRQn);
  /* USER CODE BEGIN RTC_MspDeInit 1 */

  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

//void SystemClock_Config(void)
//{
////	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
////  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
////  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

////  /** Configure the main internal regulator output voltage
////  */
////  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

////  /** Initializes the RCC Oscillators according to the specified parameters
////  * in the RCC_OscInitTypeDef structure.
////  */
////  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
////  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
////  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
////  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
////  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
////  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
////  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
////  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
////  {
////    Error_Handler();
////  }

////  /** Initializes the CPU, AHB and APB buses clocks
////  */
////  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
////                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
////  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
////  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
////  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
////  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

////  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
////  {
////    Error_Handler();
////  }
////  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPTIM1;
////  PeriphClkInit.LptimClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;

////  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
////  {
////    Error_Handler();
////  }
//  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
//  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
//  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

//  /** Configure the main internal regulator output voltage
//  */
//  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

//  /** Initializes the RCC Oscillators according to the specified parameters
//  * in the RCC_OscInitTypeDef structure.
//  */
//  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
//  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
//  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
//  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
//  {
//    Error_Handler();
//  }

//  /** Initializes the CPU, AHB and APB buses clocks
//  */
//  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
//                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
//  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
//  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
//  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
//  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

//  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_LPTIM1;
//  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
//  PeriphClkInit.LptimClockSelection = RCC_LPTIM1CLKSOURCE_PCLK;

//  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
//  {
//    Error_Handler();
//  }

//  /** Enables the Clock Security System
//  */
//  HAL_RCC_EnableCSS();
//}

/* USER CODE END 1 */
