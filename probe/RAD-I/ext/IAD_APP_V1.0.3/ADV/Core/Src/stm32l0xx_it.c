/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l0xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "gpio.h"
#include "lptim.h"
#include "control.h"
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern ADC_HandleTypeDef hadc;
//extern LPTIM_HandleTypeDef hlptim1;
//extern RTC_HandleTypeDef hrtc;
//extern UART_HandleTypeDef huart1;
/* USER CODE BEGIN EV */
//extern uint16_t LPR_Time_Cnt;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
//void SysTick_Handler(void)
//{
//  /* USER CODE BEGIN SysTick_IRQn 0 */

//  /* USER CODE END SysTick_IRQn 0 */
//  HAL_IncTick();
//  /* USER CODE BEGIN SysTick_IRQn 1 */

//  /* USER CODE END SysTick_IRQn 1 */
//}

/******************************************************************************/
/* STM32L0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles RTC global interrupt through EXTI lines 17, 19 and 20 and LSE CSS interrupt through EXTI line 19.
  */
//void RTC_IRQHandler(void)
//{
//  /* USER CODE BEGIN RTC_IRQn 0 */

//  /* USER CODE END RTC_IRQn 0 */
//  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
//  /* USER CODE BEGIN RTC_IRQn 1 */

//  /* USER CODE END RTC_IRQn 1 */
//}

/**
  * @brief This function handles RCC global interrupt.
  */
void RCC_IRQHandler(void)
{
  /* USER CODE BEGIN RCC_IRQn 0 */

  /* USER CODE END RCC_IRQn 0 */
  /* USER CODE BEGIN RCC_IRQn 1 */

  /* USER CODE END RCC_IRQn 1 */
}

/**
  * @brief This function handles EXTI line 4 to 15 interrupts.
  */
//void EXTI4_15_IRQHandler(void)
//{
//  /* USER CODE BEGIN EXTI4_15_IRQn 0 */

//  /* USER CODE END EXTI4_15_IRQn 0 */
//  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
//  /* USER CODE BEGIN EXTI4_15_IRQn 1 */

//  /* USER CODE END EXTI4_15_IRQn 1 */
//}

/**
  * @brief This function handles ADC, COMP1 and COMP2 interrupts (COMP interrupts through EXTI lines 21 and 22).
  */
void ADC1_COMP_IRQHandler(void)
{
  /* USER CODE BEGIN ADC1_COMP_IRQn 0 */

  /* USER CODE END ADC1_COMP_IRQn 0 */
  HAL_ADC_IRQHandler(&hadc);
  /* USER CODE BEGIN ADC1_COMP_IRQn 1 */

  /* USER CODE END ADC1_COMP_IRQn 1 */
}

/**
  * @brief This function handles LPTIM1 global interrupt / LPTIM1 wake-up interrupt through EXTI line 29.
  */
//void LPTIM1_IRQHandler(void)
//{
//  /* USER CODE BEGIN LPTIM1_IRQn 0 */

//  /* USER CODE END LPTIM1_IRQn 0 */
//  HAL_LPTIM_IRQHandler(&hlptim1);
//  /* USER CODE BEGIN LPTIM1_IRQn 1 */

//  /* USER CODE END LPTIM1_IRQn 1 */
//}

/**
  * @brief This function handles USART1 global interrupt / USART1 wake-up interrupt through EXTI line 25.
  */
//void USART1_IRQHandler(void)
//{
//  /* USER CODE BEGIN USART1_IRQn 0 */

//  /* USER CODE END USART1_IRQn 0 */
//  HAL_UART_IRQHandler(&huart1);
//  /* USER CODE BEGIN USART1_IRQn 1 */

//  /* USER CODE END USART1_IRQn 1 */
//}

/* USER CODE BEGIN 1 */

void EXTI0_1_IRQHandler(void)
{
	if(__HAL_GPIO_EXTI_GET_IT(KEY_INT_Pin) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(KEY_INT_Pin);
        sys_bits.run_md = EXIT_LPR_MODE;
//        Exit_LPR(KEY_BRIGHT);
    }
	 
    if(__HAL_GPIO_EXTI_GET_IT(KEY2_Pin) != RESET)
    { 
        __HAL_GPIO_EXTI_CLEAR_IT(KEY2_Pin);
        sys_bits.run_md = EXIT_LPR_MODE;
//        Exit_LPR(KEY_BRIGHT);
    }
}

void EXTI4_15_IRQHandler(void)
{
	if(__HAL_GPIO_EXTI_GET_IT(KEY1_Pin) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(KEY1_Pin);
        sys_bits.run_md = EXIT_LPR_MODE;
//        Exit_LPR(KEY_BRIGHT);
	 }

	if(__HAL_GPIO_EXTI_GET_IT(USB_DET_Pin) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(USB_DET_Pin);

        if(sys_bits.run_md == LPR_MODE)
            sys_bits.run_md = EXIT_LPR_MODE;
//            Exit_LPR(KEY_BRIGHT);
		 USB_Detect();
	 }
}
/* USER CODE END 1 */
