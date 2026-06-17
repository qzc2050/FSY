/**********************************************************************************************************
 * 文件名: dev_queue.h
 * 概  述: 设备队列管理
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

#include <stdint.h>

#define QUEUE_INVALID_IDX   0xFFFF

// 设备配置结构体
typedef struct{
    uint16_t txq_depth;     // 发送队列深度
    uint16_t rxq_depth;     // 接收队列深度
    uint16_t txb_size;      // 发送缓存大小
    uint16_t rxb_size;      // 接收缓存大小
}Dev_Queue_Config_t;

typedef struct{
    uint16_t size;          // 数据大小
    uint8_t buf[2];         // 缓存区
}Dev_Data_t;

typedef struct{
    uint16_t depth;         // 队列深度
    uint16_t count;         // 元素个数
    uint16_t head;          // 队头索引
    uint16_t tail;          // 队尾索引
}Dev_Queue_t;


void Dev_Queue_Clear(Dev_Queue_t *q);
uint16_t Dev_Get_Queue_Idle(Dev_Queue_t *q);
uint16_t Dev_Get_Queue_Occupied(Dev_Queue_t *q);
uint16_t Dev_Get_Queue_FreeCount(Dev_Queue_t *q);

void Dev_Queue_Push(Dev_Queue_t *q);
void Dev_Queue_Pop(Dev_Queue_t *q);


#ifdef __cplusplus
}
#endif
