/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"
#include "pm2_5.h"
#include "lora.h"

/* USER CODE BEGIN 0 */

//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)  
//解决HAL库使用时,某些情况可能报错的bug
int _ttywrch(int ch)    
{
    ch=ch;
	return ch;
}

//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 
	/* Whatever you require here. If the only file you are using is */ 
	/* standard output using printf() for debugging, no file handling */ 
	/* is required. */ 
}; 

/* FILE is typedef’ d in stdio.h. */ 
FILE __stdout;    

//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 

//重定义fputc函数 
int fputc(int ch, FILE *f)
{
	while((USART1->ISR & 0X40)==0);//循环发送,直到发送完毕   
	USART1->TDR = (uint8_t) ch;
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
#endif 

uint8_t  LORA_RX_BUF[LORA_REC_LEN];     //接收缓冲,最大UART_RECV_LEN个字节.末字节为换行符 
uint16_t LORA_RX_STA;         		     //接收状态标记	

uint8_t  UART_RX_BUF[UART_RECV_LEN] = {0};     //接收缓冲,最大UART_RECV_LEN个字节.末字节为换行符 
uint16_t UART_RX_STA = 0;         		        //接收状态标记
/* USER CODE END 0 */

UART_HandleTypeDef huart5;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* UART5 init function */
void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 9600;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart5, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart5, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */
  /* UART5 RX 中断由 LORA_Init() 开启（入 ring buffer）；此处不重复使能 */
  /* USER CODE END UART5_Init 2 */

}
/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 921600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
  HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(USART1_IRQn);
	
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
	__HAL_UART_CLEAR_FLAG(&huart1, UART_FLAG_RXNE);
  /* USER CODE END USART1_Init 2 */

}
/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 9600;  /* PM2.5 传感器波特率：9600bps */
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }


  /* USER CODE BEGIN USART3_Init 2 */
  /* PM2.5 传感器初始化完成 */
  HAL_NVIC_SetPriority(USART3_IRQn, 2, 0);  /* 较低优先级，避免影响其他串口 */
	HAL_NVIC_EnableIRQ(USART3_IRQn);
	
	/* 使能接收中断 */
	__HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
	/* 清除接收标志 */
	__HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_RXNE);
  /* USER CODE END USART3_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(uartHandle->Instance==UART5)
  {
  /* USER CODE BEGIN UART5_MspInit 0 */

  /* USER CODE END UART5_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART5;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* UART5 clock enable */
    __HAL_RCC_UART5_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**UART5 GPIO Configuration
    PC12     ------> UART5_TX
    PD2     ------> UART5_RX
    */
    GPIO_InitStruct.Pin = LORA_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(LORA_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LORA_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
    HAL_GPIO_Init(LORA_RX_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN UART5_MspInit 1 */

  /* USER CODE END UART5_MspInit 1 */
  }
  else if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PB11     ------> USART3_RX
    PC10     ------> USART3_TX
    */
    GPIO_InitStruct.Pin = PM25_USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(PM25_USART_RX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PM25_USART_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(PM25_USART_TX_GPIO_Port, &GPIO_InitStruct);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==UART5)
  {
  /* USER CODE BEGIN UART5_MspDeInit 0 */

  /* USER CODE END UART5_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_UART5_CLK_DISABLE();

    /**UART5 GPIO Configuration
    PC12     ------> UART5_TX
    PD2     ------> UART5_RX
    */
    HAL_GPIO_DeInit(LORA_TX_GPIO_Port, LORA_TX_Pin);

    HAL_GPIO_DeInit(LORA_RX_GPIO_Port, LORA_RX_Pin);

  /* USER CODE BEGIN UART5_MspDeInit 1 */

  /* USER CODE END UART5_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART1)
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
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PB11     ------> USART3_RX
    PC10     ------> USART3_TX
    */
    HAL_GPIO_DeInit(PM25_USART_RX_GPIO_Port, PM25_USART_RX_Pin);

    HAL_GPIO_DeInit(PM25_USART_TX_GPIO_Port, PM25_USART_TX_Pin);

    /* USART3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void UART5_IRQHandler(void)
{
    LORA_UART_IRQHandler();

#if LORA_UART5_IRQ_LEGACY
    {
    uint8_t len = 0,t;

    if(UART5->ISR & USART_ISR_RXNE_RXFNE) {
        char data = UART5->RDR;

        if ((LORA_RX_STA & 0x8000) == 0) //接收未完成
        {
            if (LORA_RX_STA & 0x4000) //接收到了0x0d
            {
                if (data != 0x0a)
                    LORA_RX_STA = 0; //接收错误,重新开始
                else
                {
                    LORA_RX_STA |= 0x8000; //接收完成了
                    len=LORA_RX_STA&0x3fff;//得到此次接收到的数据长度
                    printf("\r\n接收指令: ");
                    for(t=0;t<len;t++)
                    {
                        USART1->TDR=LORA_RX_BUF[t];
                        while((USART1->ISR & 0X40)==0);//等待发送结束
                    }
                    USART1->TDR = '\r';
                    while((USART1->ISR & 0X40)==0);//等待发送结束
                    USART1->TDR = '\n';
                    while((USART1->ISR & 0X40)==0);//等待发送结束
                    LORA_RX_STA = 0;
                }
            }
            else //还没收到0X0D
            {
                if (data == 0x0d)
                    LORA_RX_STA |= 0x4000;
                else
                {
                    LORA_RX_BUF[LORA_RX_STA & 0X3FFF] = data;
                    LORA_RX_STA++;
                    if (LORA_RX_STA > (LORA_REC_LEN - 1))
                        LORA_RX_STA = 0; //接收数据错误,重新开始接收
                }
            }
        }
    }
    }
#endif

    if(UART5->ISR & USART_ISR_ORE)
        UART5->ICR |= USART_ICR_ORECF;
}

void USART3_IRQHandler(void)
{
    uint8_t data;
    
    /* 接收数据寄存器非空 */
    if(USART3->ISR & USART_ISR_RXNE_RXFNE) {
        /* 读取 RDR 会自动清除 RXNE 标志 */
        data = USART3->RDR;
        
        /* 处理 PM2.5 数据接收 */
        PM2_5_ProcessRxByte(data);
    }

    /* 清除溢出错误标志 */
    if(USART3->ISR & USART_ISR_ORE)
        USART3->ICR |= USART_ICR_ORECF;
}

void USART1_IRQHandler(void)
{
    if(USART1->ISR & USART_ISR_RXNE_RXFNE) {
        char data = USART1->RDR;

        if ((UART_RX_STA & 0x8000) == 0) //接收未完成
        {
            if (UART_RX_STA & 0x4000) //接收到了0x0d
            {
                if (data != 0x0a)
                    UART_RX_STA = 0; //接收错误,重新开始
                else
                {
                    UART_RX_STA |= 0x8000; //接收完成了

//                    uint8_t len=UART_RX_STA&0x3fff;//得到此次接收到的数据长度
//                    printf("\r\n接收指令: ");
//                    for(uint8_t t=0;t<len;t++)
//                    {
//                        USART1->TDR=UART_RX_BUF[t];
//                        while((USART1->ISR & 0X40)==0);//等待发送结束
//                    }
//                    USART1->TDR = '\r';
//                    while((USART1->ISR & 0X40)==0);//等待发送结束
//                    USART1->TDR = '\n';
//                    while((USART1->ISR & 0X40)==0);//等待发送结束
//                    UART_RX_STA = 0;  //测试
                }
            }
            else //还没收到0X0D
            {
                if (data == 0x0d)
                    UART_RX_STA |= 0x4000;
                else
                {
                    UART_RX_BUF[UART_RX_STA & 0X3FFF] = data;
                    UART_RX_STA++;
                    if (UART_RX_STA > (UART_RECV_LEN - 1))
                        UART_RX_STA = 0; //接收数据错误,重新开始接收
                }
            }
        }
    }


    if(USART1->ISR & USART_ISR_ORE)
        USART1->ICR |= USART_ICR_ORECF;
}
/* USER CODE END 1 */
