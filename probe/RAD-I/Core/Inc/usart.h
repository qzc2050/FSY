/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
void uart_safe_printf(const char *fmt, ...);
/* USER CODE END Includes */

extern UART_HandleTypeDef huart5;

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart3;

/* USER CODE BEGIN Private defines */
#define LORA_REC_LEN        256  	//定义最大接收字节数
#define UART_RECV_LEN       100  	//定义最大接收字节数
    

extern uint8_t  LORA_RX_BUF[LORA_REC_LEN];     //接收缓冲,最大UART_RECV_LEN个字节.末字节为换行符 
extern uint16_t LORA_RX_STA;         		    //接收状态标记	

extern uint8_t  UART_RX_BUF[UART_RECV_LEN];    //接收缓冲,最大UART_RECV_LEN个字节.末字节为换行符 
extern uint16_t UART_RX_STA;         		    //接收状态标记	
/* USER CODE END Private defines */

void MX_UART5_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART3_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

