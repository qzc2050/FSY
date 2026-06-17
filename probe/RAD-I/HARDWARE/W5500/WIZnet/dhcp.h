/*******************************************************************************
 * WIZnet DHCP Library for W5500
 * 移植自：https://github.com/WIZnet/ioLibrary
 * 简化版本，仅支持 DHCP Client 基本功能
 ******************************************************************************/

#ifndef _DHCP_H_
#define _DHCP_H_

#include <stdint.h>
#include "w5500.h"
#include "socket.h"

/* DHCP 状态定义 */
#define DHCP_INIT           0
#define DHCP_DISCOVER       1
#define DHCP_REQUEST        2
#define DHCP_OFFER          3
#define DHCP_ACK            4
#define DHCP_NACK           5
#define DHCP_LEASED         6
#define DHCP_REREQUEST      7
#define DHCP_RELEASE        8

/* DHCP 超时时间 (ms) */
#define DHCP_INIT_TIMEOUT       1000    // 初始化超时
#define DHCP_DISCOVER_TIMEOUT   5000    // DISCOVER 超时
#define DHCP_REQUEST_TIMEOUT    5000    // REQUEST 超时
#define DHCP_LEASED_TIMEOUT     60000   // 租约检查间隔

/* DHCP 端口 */
#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

/* DHCP 消息类型 */
#define DHCP_DISCOVER_MSG   1
#define DHCP_OFFER_MSG      2
#define DHCP_REQUEST_MSG    3
#define DHCP_DECLINE_MSG    4
#define DHCP_ACK_MSG        5
#define DHCP_NAK_MSG        6
#define DHCP_RELEASE_MSG    7
#define DHCP_INFORM_MSG     8

/* DHCP 选项 */
#define DHCP_OPT_PAD                0
#define DHCP_OPT_SUBNET_MASK        1
#define DHCP_OPT_ROUTER             3
#define DHCP_OPT_DNS                6
#define DHCP_OPT_HOST_NAME          12
#define DHCP_OPT_REQUEST_IP         50
#define DHCP_OPT_IP_LEASE_TIME      51
#define DHCP_OPT_MSG_TYPE           53
#define DHCP_OPT_SERVER_ID          54
#define DHCP_OPT_PARAM_REQUEST      55
#define DHCP_OPT_END                255

/* DHCP 消息长度 */
#define DHCP_MSG_LEN    236

/* DHCP 成功/失败 */
#define DHCP_OK         0
#define DHCP_FAIL       -1
#define DHCP_TIMEOUT    -2
#define DHCP_RUNNING    1

/* DHCP 结构体 (使用 packed 避免对齐问题) */
typedef struct __attribute__((packed)) {
    uint8_t  op;            // 消息类型 (1=请求，2=响应)
    uint8_t  htype;         // 硬件类型 (1=以太网)
    uint8_t  hlen;          // 硬件地址长度 (6)
    uint8_t  hops;          // 跳数
    uint32_t xid;           // 事务 ID
    uint16_t secs;          // 秒数
    uint16_t flags;         // 标志
    uint8_t  ciaddr[4];     // 客户端 IP
    uint8_t  yiaddr[4];     // 你的 IP (分配的 IP)
    uint8_t  siaddr[4];     // 服务器 IP
    uint8_t  giaddr[4];     // 网关 IP
    uint8_t  chaddr[16];    // 客户端硬件地址
    uint8_t  sname[64];     // 服务器主机名
    uint8_t  file[128];     // 启动文件名
    uint8_t  options[312];  // 可选参数
} DHCP_Message_t;

/* DHCP 信息结构 (使用 packed 避免对齐问题) */
typedef struct __attribute__((packed)) {
    uint8_t  ip[4];         // 分配的 IP
    uint8_t  subnet[4];     // 子网掩码
    uint8_t  gateway[4];    // 网关
    uint8_t  dns[4];        // DNS 服务器
    uint8_t  dhcp_server[4];// DHCP 服务器 IP
    uint32_t lease_time;    // 租约时间 (秒)
    uint32_t xid;           // 事务 ID
} DHCP_Info_t;

/* 函数声明 */
int8_t DHCP_Init(uint8_t socket_num);
int8_t  DHCP_Run(void);
uint8_t DHCP_Get_State(void);
void    DHCP_Set_State(uint8_t state);
uint8_t DHCP_Get_IP_Info(DHCP_Info_t *info);
void    DHCP_Stop(void);

#endif /* _DHCP_H_ */
