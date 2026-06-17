/**********************************************************************************************************
 * 文件名: spi_bsp.c
 * 概  述: SPI协议底层接口
 * 创建时间: 2025-08-01
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#include "./spi/spi_bsp.h"
#include "./spi/spi_protocol.h"


//! ---------------- ↓ 自定义头文件 ↓ ---------------- !//
#include "spi.h"
//! ---------------- ↑ 自定义头文件 ↑ ---------------- !//


//! ----------------- ↓ 自定义变量 ↓ ----------------- !//
Spi_Periph_t *spi1_ph = NULL;
Spi_Device_t *esp32c6_dh = NULL;
//! ----------------- ↑ 自定义变量 ↑ ----------------- !//


/********************************************************************************************
* 函数名：Spi_Device_Init
* 描  述：SPI 设备初始化
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Spi_Device_Init(void)
{
    //! SPI 外设注册 / 设备注册（自定义）
    Dev_Queue_Config_t qcfg = {
        .txb_size = 1024,
        .txq_depth = 12,
        .rxb_size = 1024,
        .rxq_depth = 12,
    };
    
    spi1_ph = Spi_Periph_Register("SPI1", Ph_Spi1_Init, Ph_Spi1_DeInit, Spi1_TxRx_Data);
    esp32c6_dh = Spi_Device_Register(spi1_ph, 
                                    "ESP32C6", 
                                    Esp32c6_Spi_Cs_Enable, 
                                    Esp32c6_Spi_Cs_Disable, 
                                    qcfg,
                                    4);
}


//! ---------------- ↓ 自定义函数 ↓ ---------------- !//
/********************************************************************************************
* 函数名：Ph_Spi1_Init
* 描  述：SPI1 外设初始化
* 输  入：无
* 输  出：@retval: true -> 初始化成功，false -> 初始化失败
* 调  用：外部调用
********************************************************************************************/
bool Ph_Spi1_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 0x0;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    // hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    // hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    // hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    // hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    // hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    // hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    // hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;

    if(HAL_SPI_Init(&hspi1) != HAL_OK)
        return false;
    return true;
}

/********************************************************************************************
* 函数名：Ph_Spi1_DeInit
* 描  述：SPI1 外设反初始化
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Ph_Spi1_DeInit(void)
{
    
}

/********************************************************************************************
* 函数名：Spi1_TxRx_Data
* 描  述：SPI1 数据传输
* 输  入：@param: *sdata -> 数据指针
*         @param: *rdata -> 数据指针
*         @param: len -> 数据长度
* 输  出：@retval: true -> 发送成功
*         @retval: false -> 发送失败
* 调  用：外部调用
********************************************************************************************/
bool Spi1_TxRx_Data(uint8_t *sdata, uint8_t *rdata, uint16_t len)
{
    bool txrx_flag = false;
    
    //! DMA模式同样的延时和阻塞模式相比，反而容易丢包
//    if(HAL_SPI_TransmitReceive_DMA(&hspi1, sdata, rdata, len) == HAL_OK)
    if(HAL_SPI_TransmitReceive(&hspi1, sdata, rdata, len, 0xffff) == HAL_OK)
        txrx_flag = true;
    return txrx_flag;
}

/********************************************************************************************
* 函数名：Esp32c6_Spi_Cs_Enable
* 描  述：ESP32C6 设备CS拉低
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Esp32c6_Spi_Cs_Enable(void)
{
    HAL_GPIO_WritePin(ESP32_CS_GPIO_Port, ESP32_CS_Pin, GPIO_PIN_RESET);
}

/********************************************************************************************
* 函数名：Esp32c6_Spi_Cs_Disable
* 描  述：ESP32C6 设备CS拉高
* 输  入：无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Esp32c6_Spi_Cs_Disable(void)
{
    HAL_GPIO_WritePin(ESP32_CS_GPIO_Port, ESP32_CS_Pin, GPIO_PIN_SET);
}
//! ---------------- ↑ 自定义函数 ↑ ---------------- !//






