/**********************************************************************************************************
 * 文件名: net_raw_config.h
 * 描  述: 网络/串口裸协议配置（辐射报警仪 Modbus RTU 帧）
 * 创建时间: 2026-03-30
 * 更新时间: 2026-03-30
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
/********************************** 辐射报警仪协议（网口/串口/UDP 承载） **************************
 * 帧格式：内机地址 | 功能码 | 数据域 | CRC 低位 | CRC 高位
 * CRC：对 CRC 前所有字节按 Modbus RTU（多项式 0xA001，初值 0xFFFF，LSB 先发）
 * 说明见 docs / tmpdocs 协议摘录（功能码 0x06/0x16/0x86、0x10/0x20/0x90、0x05/0x15/0x85、0x03/0x13/0x23/0x83 等）
*************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "./core/dev_config.h"
#include "./net_raw/net_raw_bsp.h"


//! ----------------------- ↓ NET 测试功能 ↓ ----------------------- !//
#define NET_SEND_DEBUG              (true)
#if NET_SEND_DEBUG
#define NET_SEND_PRINTF(n,args...)  DEV_PRINTF(n,##args)
#else
#define NET_SEND_PRINTF(n,args...)
#endif

#define NET_RECV_DEBUG              (false)
#if NET_RECV_DEBUG
#define NET_RECV_PRINTF(n,args...)  DEV_PRINTF(n,##args)
#else
#define NET_RECV_PRINTF(n,args...)
#endif

#define NET_MASTER_RESP_DEBUG       (false)
#if NET_MASTER_RESP_DEBUG
#define NET_MASTER_RESP_PRINTF(n,args...)  DEV_PRINTF(n,##args)
#else
#define NET_MASTER_RESP_PRINTF(n,args...)
#endif
//! ----------------------- ↑ NET 测试功能 ↑ ----------------------- !//


//! ----------------------- ↓ NET 功能配置 ↓ ----------------------- !//
#define NET_TRANSMIT_ACK            (true)     // 打开：发送后等待应答帧（地址/功能码匹配），超时重发
#define NET_SEND_ERR_HANDLE         (false)    // 发送错误处理

//! ----------------------- ↑ NET 功能配置 ↑ ----------------------- !//


//! ----------------------- ↓ NET 设备配置 ↓ ----------------------- !//
#define NET_MAX_PERIPH_CNT          (3)        // 可注册外设数（W5500 / CAN / LORA）
#define NET_MAX_DEV_CNT             (3)        // 可注册设备数（取值范围：1 ~ 255）
#define NET_NAME_LEN                (16)       // 名称长度（外设/设备）
#define NET_RAW_SLAVE_ADDR_DEFAULT  (0x01)     // 默认从机地址
//! ----------------------- ↑ NET 设备配置 ↑ ----------------------- !//


//! ---------------------- ↓ NET 协议帧配置（Modbus RTU） ↓ ---------------------- !//
#define NET_CRC_LENTH               (2)        // CRC 长度（单位：字节，低位在前）
//! ----------------------- ↑ NET 协议帧配置 ↑ ----------------------- !//


//! ------------------------ ↓ NET 应答配置（仅 NET_TRANSMIT_ACK=1 时有效） ↓ ------------------------ !//
#define NET_RETX_TIMES              (2)
#define NET_ACK_WAITING_TIME        (32)
//! ------------------------ ↑ NET 应答配置 ↑ ------------------------ !//


#if (!NET_MAX_PERIPH_CNT || !NET_MAX_DEV_CNT)
#error "可注册外设/设备数不可设置为0！"
#endif

#if (!NET_NAME_LEN)
#error "名称长度不可设置为0！"
#endif


#ifdef __cplusplus
}
#endif
