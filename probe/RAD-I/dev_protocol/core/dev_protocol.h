/**********************************************************************************************************
 * 文件名: dev_protocol.h
 * 概  述: 设备初始化
 * 创建时间: 2025-08-14
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "./core/dev_config.h"


extern void Dev_Protocol_init(void);
extern uint16_t Dev_Calculate_CRC(uint8_t *data, uint32_t size);
extern uint16_t Dev_Id_alloc(void);
extern void Dev_Id_Release(uint16_t id);
extern void Dev_Tk_Init(uint32_t *time_tk);
extern bool Dev_Tk_Wait(uint32_t time, uint32_t time_tk);


#ifdef __cplusplus
}
#endif
