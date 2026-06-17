/***********************************************************************************
成都浩然电子有限公司
W5500 官方代理商
电话：028-86127089     0755-86066647
网址：http://www.hschip.com

硬件平台：浩然电子评估板 HS-EVBW5500 /STM32
W5500 技术交流 QQ 群：722479032

功能：W5500 动态 IP 分配（DHCP）与 UDP 广播
***********************************************************************************/

#ifndef _W5500_DHCP_H_
#define _W5500_DHCP_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ==================== UDP 组播发现（见 tmpdocs 协议：236.2.3.6:2468） ==================== */
#define UDP_DISCOVER_MULTICAST_IP   {236, 2, 3, 6}
#define UDP_DISCOVER_MULTICAST_PORT (2468)
#define UDP_BROADCAST_INTERVAL_MS     (1000)         /* 发送间隔 1 秒 */
#define UDP_BROADCAST_SOCKET          (2)            /* 使用 Socket 2 */
#define UDP_DISCOVER_PAYLOAD_MAX      (192)          /* 逗号分隔 ASCII 一行 */

/* 兼容旧名：发现发往组播地址（非 255.255.255.255 全局广播） */
#define BROADCAST_IP_ADDR           UDP_DISCOVER_MULTICAST_IP

/* ==================== DHCP 配置 ==================== */
#define DHCP_SOCKET                 (3)              // DHCP 专用 Socket
#define DHCP_CHECK_INTERVAL_MS      (1000)           // IP 检查间隔 1 秒
#define DHCP_MAX_RETRY              (3)              // DHCP 最大重试次数
#define DHCP_TIMEOUT_MS             (5000)           // DHCP 超时 5 秒
#define DHCP_RETRY_DELAY_MS         (5000)           // 重试间隔 5 秒

/* DHCP 看门狗：开机 40s 后开始检测，满 50s 仍无有效 IP 则硬复位 W5500，之后每 10s 检测 */
#define DHCP_WDG_ARM_MS             (40000U)
#define DHCP_WDG_FIRST_WAIT_MS      (10000U)
#define DHCP_WDG_PERIOD_MS          (10000U)

/* DHCP 状态机 */
typedef enum
{
    DHCP_STATE_OFF = 0,           // DHCP 未启用
    DHCP_STATE_INIT,              // 初始化状态
    DHCP_STATE_DISCOVER,          // 发送 DISCOVER
    DHCP_STATE_REQUEST,           // 发送 REQUEST
    DHCP_STATE_BOUND,             // 已获取 IP
    DHCP_STATE_TIMEOUT,           // 超时
    DHCP_STATE_FAILED             // 失败
} DHCP_State_t;

/* ==================== 函数声明 ==================== */

/**
 * @brief  W5500 DHCP 初始化（配置为动态 IP）
 * @param  无
 * @retval 无
 */
void W5500_DHCP_Init(void);

/**
 * @brief  DHCP 客户端任务（周期性调用，处理 DHCP 状态机）
 * @param  无
 * @retval 当前 DHCP 状态
 */
DHCP_State_t DHCP_Client_Task(void);

/**
 * @brief  获取当前 DHCP 状态
 * @param  无
 * @retval DHCP 状态
 */
DHCP_State_t DHCP_Get_State(void);

/**
 * @brief  重置 DHCP 状态（重新获取 IP）
 * @param  无
 * @retval 无
 */
void DHCP_Reset(void);

/**
 * @brief  DHCP 看门狗（DHCP 模式下周期检测 IP，无效时复位 W5500）
 * @note   仅在 W5500_Task 中调用
 */
void DHCP_Watchdog_Task(void);

/**
 * @brief  W5500 是否正在执行网络硬复位/重初始化
 */
bool W5500_Is_Network_Recovering(void);

/**
 * @brief  获取当前 IP 地址
 * @param  ip_addr : IP 地址缓冲区 (4 字节)
 * @retval 无
 */
void W5500_Get_IP(uint8_t *ip_addr);

/**
 * @brief  检查 IP 是否有效（非 0.0.0.0）
 * @param  无
 * @retval true  : IP 有效
 * @retval false : IP 无效（0.0.0.0）
 */
bool W5500_Is_IP_Valid(void);

/**
 * @brief  UDP 广播初始化
 * @param  无
 * @retval 无
 */
void UDP_Broadcast_Init(void);

/**
 * @brief  发送 UDP 广播数据
 * @param  data : 数据指针
 * @param  len  : 数据长度
 * @retval 实际发送字节数
 */
uint16_t UDP_Broadcast_Send(const uint8_t *data, uint16_t len);

/**
 * @brief  UDP 广播任务（需在 FreeRTOS 任务中调用）
 * @param  无
 * @retval 无
 */
void UDP_Broadcast_Task(void);

/**
 * @brief  填充广播数据（产品型号、序列号、IP 地址等）
 * @param  buf : 数据缓冲区
 * @retval 填充的数据长度
 */
uint16_t Broadcast_Fill_Data(uint8_t *buf);

#endif /* _W5500_DHCP_H_ */
