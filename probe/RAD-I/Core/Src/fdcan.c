/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    fdcan.c
  * @brief   This file provides code for the configuration
  *          of the FDCAN instances.
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
#include <stdbool.h>
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "fdcan.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

FDCAN_HandleTypeDef hfdcan2;

/* FDCAN2 init function */
void MX_FDCAN2_Init(void)
{

  /* USER CODE BEGIN FDCAN2_Init 0 */

  /* USER CODE END FDCAN2_Init 0 */

//  FDCAN_ClkCalUnitTypeDef sCcuConfig = {0};

  /* USER CODE BEGIN FDCAN2_Init 1 */

  /* USER CODE END FDCAN2_Init 1 */
  hfdcan2.Instance = FDCAN2;
  hfdcan2.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan2.Init.AutoRetransmission = ENABLE;
  hfdcan2.Init.TransmitPause = DISABLE;
  hfdcan2.Init.ProtocolException = DISABLE;
  hfdcan2.Init.NominalPrescaler = 15;
  hfdcan2.Init.NominalSyncJumpWidth = 2;
  hfdcan2.Init.NominalTimeSeg1 = 13;
  hfdcan2.Init.NominalTimeSeg2 = 2;
  hfdcan2.Init.DataPrescaler = 15;
  hfdcan2.Init.DataSyncJumpWidth = 2;
  hfdcan2.Init.DataTimeSeg1 = 13;
  hfdcan2.Init.DataTimeSeg2 = 2;
  hfdcan2.Init.MessageRAMOffset = 0;
  hfdcan2.Init.StdFiltersNbr = 1;
  hfdcan2.Init.ExtFiltersNbr = 0;
  hfdcan2.Init.RxFifo0ElmtsNbr = 32;
  hfdcan2.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan2.Init.RxFifo1ElmtsNbr = 0;
  hfdcan2.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan2.Init.RxBuffersNbr = 0;
  hfdcan2.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan2.Init.TxEventsNbr = 0;
  hfdcan2.Init.TxBuffersNbr = 0;
  hfdcan2.Init.TxFifoQueueElmtsNbr = 32;
  hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan2.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK)
  {
    Error_Handler();
  }
//  sCcuConfig.ClockDivider = FDCAN_CLOCK_DIV1;
//  sCcuConfig.MinOscClkPeriods = 0x00;
//  sCcuConfig.CalFieldLength = FDCAN_CALIB_FIELD_LENGTH_32;
//  sCcuConfig.TimeQuantaPerBitTime = 4;
//  sCcuConfig.WatchdogStartValue = 0x0000;
//  if (HAL_FDCAN_ConfigClockCalibration(&hfdcan2, &sCcuConfig) != HAL_OK)
//  {
//    Error_Handler();
//  }
  /* USER CODE BEGIN FDCAN2_Init 2 */

  /* USER CODE END FDCAN2_Init 2 */

}

void HAL_FDCAN_MspInit(FDCAN_HandleTypeDef* fdcanHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(fdcanHandle->Instance==FDCAN2)
  {
  /* USER CODE BEGIN FDCAN2_MspInit 0 */

  /* USER CODE END FDCAN2_MspInit 0 */
    /* FDCAN2 clock enable */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**FDCAN2 GPIO Configuration
    PB12     ------> FDCAN2_RX
    PB13     ------> FDCAN2_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN FDCAN2_MspInit 1 */

  /* USER CODE END FDCAN2_MspInit 1 */
  }
}

void HAL_FDCAN_MspDeInit(FDCAN_HandleTypeDef* fdcanHandle)
{

  if(fdcanHandle->Instance==FDCAN2)
  {
  /* USER CODE BEGIN FDCAN2_MspDeInit 0 */

  /* USER CODE END FDCAN2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_FDCAN_CLK_DISABLE();

    /**FDCAN2 GPIO Configuration
    PB12     ------> FDCAN2_RX
    PB13     ------> FDCAN2_TX
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_12|GPIO_PIN_13);

  /* USER CODE BEGIN FDCAN2_MspDeInit 1 */

  /* USER CODE END FDCAN2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/********************************************************************************************
* 函数名：CAN_Drive_Filter_Init
* 描  述：CAN 过滤器初始化
* 输  入：无
* 输  出：true -> 过滤器配置成功，false -> 过滤器配置失败
* 调  用：外部调用
********************************************************************************************/
bool CAN_Drive_Filter_Init(void)
{
  #define CAN_ALL_FILTER_NUM  1
  
  uint8_t i;
  FDCAN_FilterTypeDef can_RxFilter = {0};
  const static FDCAN_FilterTypeDef Filter_CFG[CAN_ALL_FILTER_NUM][7] = {                    \
    {FDCAN_STANDARD_ID, 0, FDCAN_FILTER_MASK, FDCAN_FILTER_TO_RXFIFO0, 0x000, 0xF00, 0, 0}
  };

  // CAN 过滤器配置
  for(i = 0; i < CAN_ALL_FILTER_NUM; i++)
  {
      can_RxFilter.IdType = Filter_CFG[i]->IdType;                        // ID类型
      can_RxFilter.FilterIndex = Filter_CFG[i]->FilterIndex;              // 过滤器索引
      can_RxFilter.FilterType = Filter_CFG[i]->FilterType;                // 过滤器类型
      can_RxFilter.FilterConfig = Filter_CFG[i]->FilterConfig;            // 过滤器关联到队列
      can_RxFilter.FilterID1 = Filter_CFG[i]->FilterID1;                  // ID1
      can_RxFilter.FilterID2 = Filter_CFG[i]->FilterID2;                  // FDCAN_FILTER_MASK模式下，ID2为掩码
      can_RxFilter.RxBufferIndex = Filter_CFG[i]->RxBufferIndex;          // RxBuffer索引
      can_RxFilter.IsCalibrationMsg = Filter_CFG[i]->IsCalibrationMsg;    // 是否为校准消息
      if(HAL_FDCAN_ConfigFilter(&hfdcan2,&can_RxFilter) != HAL_OK) 	// 过滤器初始化
          return false;
  }

  /* 
    * 参数2：接收的标准帧ID和过滤器设置的ID不匹配时，是否拒绝接收(不匹配时,可选择放入FIFOx)。
    * 参数3：接收的扩展帧ID和过滤器设置的ID不匹配时，是否拒绝接收(不匹配时,可选择放入FIFOx)。
    * 参数4：设置是否拒绝远程标准帧。
    * 参数5：设置是否拒绝远程扩展帧。
    */
  // 全局过滤器标准帧接收配置（（ID不匹配）拒收：FDCAN_REJECT，存入FIFO0：FDCAN_ACCEPT_IN_RX_FIFO0，存入FIFO1：FDCAN_ACCEPT_IN_RX_FIFO1）
  #define CAN_GLOBAL_FILTER_STD_ID_RECV               (FDCAN_REJECT)
  // 全局过滤器扩展帧接收配置（（ID不匹配）拒收：FDCAN_REJECT，存入FIFO0：FDCAN_ACCEPT_IN_RX_FIFO0，存入FIFO1：FDCAN_ACCEPT_IN_RX_FIFO1）
  #define CAN_GLOBAL_FILTER_EXT_ID_RECV               (FDCAN_REJECT)
  // 全局过滤器远程标准帧接收配置（拒收：FDCAN_REJECT_REMOTE，接收：FDCAN_FILTER_REMOTE）
  #define CAN_GLOBAL_FILTER_STD_REMOTE_FRAME_RECV     (FDCAN_FILTER_REMOTE)
  // 全局过滤器远程扩展帧接收配置（拒收：FDCAN_REJECT_REMOTE，接收：FDCAN_FILTER_REMOTE）
  #define CAN_GLOBAL_FILTER_EXT_REMOTE_FRAME_RECV     (FDCAN_FILTER_REMOTE)
  if(HAL_FDCAN_ConfigGlobalFilter(&hfdcan2,CAN_GLOBAL_FILTER_STD_ID_RECV, \
                                            CAN_GLOBAL_FILTER_EXT_ID_RECV, \
                                            CAN_GLOBAL_FILTER_STD_REMOTE_FRAME_RECV, \
                                            CAN_GLOBAL_FILTER_EXT_REMOTE_FRAME_RECV) != HAL_OK) /* 配置FDCAN全局过滤器  */
    return false;

  return true;
}

/* USER CODE END 1 */
