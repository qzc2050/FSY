/**********************************************************************************************************
 * 文件名: spi_app.h
 * 概  述: SPI协议应用
 * 创建时间: 2025-08-01
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "./spi/spi_protocol.h"


//! ------------------------ ↓ 自定义变量/函数 ↓ ----------------------- !//
extern void Spi_Send_SN(void);
//! ------------------------ ↑ 自定义变量/函数 ↑ ----------------------- !//


extern void Spi_Resolve_Handle(uint16_t id, uint8_t *rdata, uint16_t size);
extern void Spi_Send_Err_Handle(uint16_t id, uint8_t *sdata, uint16_t size);


#ifdef __cplusplus
}
#endif
