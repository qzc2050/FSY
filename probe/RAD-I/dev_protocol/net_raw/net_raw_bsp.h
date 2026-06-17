/**********************************************************************************************************
 * 文件名: net_raw_bsp.h
 * 概  述: 网络/串口裸协议底层接口
 * 创建时间: 2026-03-30
 * 更新时间: 2026-05-20
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "./net_raw/net_raw_config.h"


//! ---------------- ↓ 自定义头文件 ↓ ---------------- !//
#include "network_cmd.h"
//! ---------------- ↑ 自定义头文件 ↑ ---------------- !//


//! ----------------- ↓ 自定义变量 ↓ ----------------- !//
#define NET_TCP_STALE_MS  5000u    /* 对端 CON 距本次 CON 超过此值才视为僵尸会话并 reset */
#define NET_TCP_SYN_HOLD_MS  5000u /* SYNRECV/SYNSENT 持续超过此值才强制 recover */



extern uint8_t g_tx_sock_ready;
extern uint8_t g_rx_sock_ready;
//! ----------------- ↑ 自定义变量 ↑ ----------------- !//


//! ----------------- ↓ 自定义函数（与 net_raw_bsp.c 实现顺序一致）↓ ----------------- !//
extern void Net_Device_Init(void);
extern bool Ph_Net_Init(void);
extern void Ph_Net_DeInit(void);
extern void Ph_Net_RebindOnIpChange(void);
extern void Net_Tcp_PeriodicMaintain(void);
extern bool Ph_Net_Transmit(uint8_t *sdata, uint16_t tx_len);
extern uint16_t Ph_Net_Receive(uint8_t *rdata);
extern bool Net_Tcp_DataSendReady(void);  /* 数据发送链路是否已连接 */

extern bool Ph_Can_Init(void);
extern void Ph_Can_DeInit(void);
extern bool Ph_Can_Transmit(uint8_t *sdata, uint16_t tx_len);
extern uint16_t Ph_Can_Receive(uint8_t *rdata);

extern bool Ph_Lora_Init(void);
extern void Ph_Lora_DeInit(void);
extern bool Ph_Lora_Transmit(uint8_t *sdata, uint16_t tx_len);
extern uint16_t Ph_Lora_Receive(uint8_t *rdata);

extern bool CAN_Drive_Filter_Init(void);
extern void Net_Device_Update_Addr(void);  // 更新协议设备地址
//! ----------------- ↑ 自定义函数 ↑ ----------------- !//


#ifdef __cplusplus
}
#endif
