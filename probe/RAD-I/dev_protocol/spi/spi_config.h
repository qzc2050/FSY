/**********************************************************************************************************
 * 文件名: spi_config.h
 * 描  述: SPI协议配置
 * 创建时间: 2025-08-01
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/********************************** SPI数据传输协议（无ACK） **************************
 * 数据格式: （帧头 + 数据长度 + 数据 + CRC校验 + 帧尾）* n
*************************************************************************************/
/********************************** SPI数据传输协议（ACK） ****************************
 * 数据格式: 帧头 + 数据长度 + 数据 + CRC校验 + 帧尾
 * 应答格式：帧头 + 0x0000 + CRC校验 + 帧尾
*************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "./core/dev_config.h"
#include "./spi/spi_bsp.h"


//! ----------------------- ↓ SPI测试功能 ↓ ----------------------- !//
// 发送测试
#define SPI_SEND_DEBUG              (false)
#if SPI_SEND_DEBUG
#define SPI_SEND_PRINTF(n,args...)  DEV_PRINTF(n,##args)
#else
#define SPI_SEND_PRINTF(n,args...)
#endif

// 接收测试
#define SPI_RECV_DEBUG              (false)
#if SPI_RECV_DEBUG
#define SPI_RECV_PRINTF(n,args...)  DEV_PRINTF(n,##args)
#else
#define SPI_RECV_PRINTF(n,args...)
#endif
//! ----------------------- ↑ SPI测试功能 ↑ ----------------------- !//


//! ----------------------- ↓ SPI功能配置 ↓ ----------------------- !//
#define SPI_TRANSMIT_ACK            (true)     // 数据传输应答
#define SPI_SEND_ERR_HANDLE         (false)    // 发送错误处理
//! ----------------------- ↑ SPI功能配置 ↑ ----------------------- !//


//! ----------------------- ↓ SPI设备配置 ↓ ----------------------- !//
#define SPI_MAX_PERIPH_CNT          (4)        // 可注册外设数（本地SPI）（取值范围：1 ~ 255）
#define SPI_MAX_DEV_CNT             (4)        // 可注册设备数（远端SPI）（取值范围：1 ~ 255）
#define SPI_NAME_LEN                (16)       // 名称长度（外设/设备）
//! ----------------------- ↑ SPI设备配置 ↑ ----------------------- !//


//! ---------------------- ↓ SPI协议帧配置 ↓ ---------------------- !//
//! 协议帧头配置
#define SPI_HEAD_1                  (0x76)
#define SPI_HEAD_2                  (0x54)
#define SPI_HEAD_3                  (0x32)
#define SPI_HEAD_4                  (0x10)

//! 协议帧尾配置
#define SPI_TAIL_1                  (0xfe)
#define SPI_TAIL_2                  (0xdc)
#define SPI_TAIL_3                  (0xba)
#define SPI_TAIL_4                  (0x98)
//! ----------------------- ↑ SPI协议帧配置 ↑ ----------------------- !//


//! ------------------------ ↓ SPI应答配置 ↓ ------------------------ !//
#define SPI_ACK_INFO                (false)    // TX/RX 应答测试
#define SPI_RETX_TIMES              (2)        //（应答超时）重发次数
#define SPI_ACK_WAITING_TIME        (16)       //（应答超时）等待时间（单位：ms）
//! ------------------------ ↑ SPI应答配置 ↑ ------------------------ !//


#if (!SPI_MAX_PERIPH_CNT || !SPI_MAX_DEV_CNT)
#error "可注册外设/设备数不可设置为0！"
#endif

#if (!SPI_NAME_LEN)
#error "名称长度不可设置为0！"
#endif

#if (SPI_DATA_LENTH > (65535 - 1))
#error "数据最大长度不能超过(65535 - 1)个字节！"
#endif


#ifdef __cplusplus
}
#endif
