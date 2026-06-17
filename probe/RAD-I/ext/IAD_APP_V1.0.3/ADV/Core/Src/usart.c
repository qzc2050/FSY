/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "main.h"
#include <stdio.h>
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */

//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目
__IO uint16_t USART1_RX_STA = 0;    //接收状态标记
__IO uint8_t USART1_RX_BUF[USART_REC_LEN];    //接收缓冲,最大USART_REC_LEN个字节.

/* USER CODE END 0 */

UART_HandleTypeDef huart1;

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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
	__HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_RXNE);
  /* USER CODE END USART1_Init 2 */

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

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
//    GPIO_InitStruct.Pin = GPIO_PIN_9;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//      
//    GPIO_InitStruct.Pin = GPIO_PIN_10;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_PULLUP;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
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

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/**
  * 函数功能: 判断USARTx是否有数据接收
  * 输入参数: USARTx
  * 返 回 值: RESET or SET
  * 说    明：无
  */
ITStatus USART_RX_STA(USART_TypeDef* USARTx)
{
	uint32_t bitpos = 0x00, itmask = 0x00;
	ITStatus bitstatus = RESET;
	
	itmask = 0x0525 & 0x001F;
	itmask = (uint32_t)0x01 << itmask;
	itmask &= USARTx->CR1; 
	
	bitpos = 0x0525 >> 0x08;
	bitpos = (uint32_t)0x01 << bitpos;
	bitpos &= USARTx->ISR;
	
	if((itmask != (uint16_t)RESET)&&(bitpos != (uint16_t)RESET))
        bitstatus = SET;
    else
        bitstatus = RESET;
	return bitstatus;
}


void USART1_IRQHandler(void)
{
	uint8_t t;
    uint8_t value,len = 0;
    
	if(USART_RX_STA(USART1) != RESET)
    {
		value = USART1->RDR;
//		USART1->TDR = value;
		if((USART1_RX_STA & 0x8000) == 0)
		{
            //接收未完成
			if(USART1_RX_STA & 0x4000)
			{
                //接收到了0x0d
				if(value != 0x0a)
					USART1_RX_STA = 0; //接收错误,重新开始
				else
				{
					USART1_RX_STA |= 0x8000; //接收完成了
					len=USART1_RX_STA&0x3fff;//得到此次接收到的数据长度
//					printf("CMD: ");
//					for(t=0;t<len;t++)
//					{
//						USART1->TDR=USART1_RX_BUF[t];
//						while((USART1->ISR & 0X40)==0);//等待发送结束
//					}
//                    printf("\r\n");
				}
			}
			else
			{ 
                //还没收到0X0D
				if (value == 0x0d)
					USART1_RX_STA |= 0x4000;
				else
				{
					USART1_RX_BUF[USART1_RX_STA & 0X3FFF] = value;
					
					USART1_RX_STA++;
					if (USART1_RX_STA > (USART_REC_LEN - 1))
						USART1_RX_STA = 0; //接收数据错误,重新开始接收
				}
			}
		}
    }
	__HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_RXNE);
	__HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_ORE);
}

/**
  * 函数功能: 重定向c库函数printf到DEBUG_USARTx
  * 输入参数: 无
  * 返 回 值: 无
  * 说    明：无
  */
int fputc(int ch, FILE *f)
{
	USART1->TDR=ch;
	while((USART1->ISR&0X40)==0);//等待发送结束
	return ch;
}

/**
  * 函数功能: 重定向c库函数getchar,scanf到DEBUG_USARTx
  * 输入参数: 无
  * 返 回 值: 无
  * 说    明：无
  */
int fgetc(FILE * f)
{
  uint8_t ch = 0;
  ch = USART1->RDR;
	while((USART1->ISR&0X40)==0);//等待发送结束
  return ch;
}
/* USER CODE END 1 */
