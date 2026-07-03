/***********************************************************************************
成都浩然电子有限公司
W5500 官方代理商

功能：W5500 动态 IP 分配（DHCP）与 UDP 广播实现
***********************************************************************************/

#include "w5500_dhcp.h"
#include "w5500.h"
#include "socket.h"
#include "device.h"
#include "config.h"
#include "network_cmd.h"
#include "net_config.h"
#include "net_tcp.h"
#include "WIZnet/dhcp.h"
#include "device_config.h"
#include "fsy_dispatch.h"

#include <string.h>
#include <stdio.h>

/* ==================== 全局变量 ==================== */

/* DHCP 状态机 */
static DHCP_State_t g_dhcp_state = DHCP_STATE_OFF;
static uint8_t g_dhcp_retry_cnt = 0;

/* 广播定时器 */
static uint32_t g_broadcast_last_tick = 0;

/* 广播使能标志 */
static uint8_t g_broadcast_enabled = 0;

/* 绑定 IP 缓存（DHCP 变更 / 硬复位后清零） */
static uint8_t s_tcp_bound_ip[4] = {0, 0, 0, 0};
static uint8_t s_udp_bound_ip[4] = {0, 0, 0, 0};

/* 网络硬复位进行中（其他路径应跳过 W5500 收发） */
static volatile uint8_t g_w5500_recovering = 0;

/* 当前 IP 地址缓存 */
//static uint8_t g_current_ip[4] = {0, 0, 0, 0};

/* 组播发现目标 IP（协议：236.2.3.6） */
static const uint8_t g_discover_mcast_ip[4] = UDP_DISCOVER_MULTICAST_IP;

/* 外部 MAC 地址（从 ConfigMsg 获取） */
extern CONFIG_MSG ConfigMsg;

/* ==================== 内部函数 ==================== */

/**
 * @brief 组播 IP 转以太网 MAC：01:00:5E:(ip[1]&7F):ip[2]:ip[3]
 */
static void UDP_Set_Multicast_Mac(SOCKET s, const uint8_t *ip)
{
    if (ip == NULL)
        return;

    IINCHIP_WRITE(Sn_DHAR0(s), 0x01);
    IINCHIP_WRITE(Sn_DHAR1(s), 0x00);
    IINCHIP_WRITE(Sn_DHAR2(s), 0x5E);
    IINCHIP_WRITE(Sn_DHAR3(s), (uint8_t)(ip[1] & 0x7Fu));
    IINCHIP_WRITE(Sn_DHAR4(s), ip[2]);
    IINCHIP_WRITE(Sn_DHAR5(s), ip[3]);
}

/**
 * @brief 设置 UDP 目标 IP/端口及组播 MAC（sendto 前必须配置 Sn_DHAR）
 */
static void UDP_Set_Destination(SOCKET s, const uint8_t *ip, uint16_t port)
{
    IINCHIP_WRITE(Sn_DIPR0(s), ip[0]);
    IINCHIP_WRITE(Sn_DIPR1(s), ip[1]);
    IINCHIP_WRITE(Sn_DIPR2(s), ip[2]);
    IINCHIP_WRITE(Sn_DIPR3(s), ip[3]);
    IINCHIP_WRITE(Sn_DPORT0(s), (uint8_t)((port & 0xff00u) >> 8));
    IINCHIP_WRITE(Sn_DPORT1(s), (uint8_t)(port & 0x00ffu));
    UDP_Set_Multicast_Mac(s, ip);
}

/* ==================== 内部函数声明 ==================== */
static void DHCP_Check_Link_Status(void);
static void W5500_DHCP_Network_Recover(void);

bool W5500_Is_Network_Recovering(void)
{
    return (g_w5500_recovering != 0U);
}

/********************************************************************************************
* 函数名：W5500_DHCP_Network_Recover
* 描  述：DHCP 长时间未拿到 IP 时，关闭 Socket、硬件复位 W5500 并重新初始化
* 注  意：仅在 W5500_Task 内调用；恢复期间 g_w5500_recovering 置位，避免其他路径 SPI 竞态
********************************************************************************************/
static void W5500_DHCP_Network_Recover(void)
{
    uint8_t ip[4];

    if(g_w5500_recovering)
        return;

    g_w5500_recovering = 1U;

    getSIPR(ip);
    printf("[DHCP-WDG] IP=%u.%u.%u.%u link UP but invalid, reset W5500...\r\n",
           (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);

    Net_Tcp_DeInit();

    g_broadcast_enabled = 0U;
    close(UDP_BROADCAST_SOCKET);
    s_udp_bound_ip[0] = 0U;
    s_udp_bound_ip[1] = 0U;
    s_udp_bound_ip[2] = 0U;
    s_udp_bound_ip[3] = 0U;
    s_tcp_bound_ip[0] = 0U;
    s_tcp_bound_ip[1] = 0U;
    s_tcp_bound_ip[2] = 0U;
    s_tcp_bound_ip[3] = 0U;

    DHCP_Stop();

    Reset_W5500();
    set_w5500_network();

    g_dhcp_state = DHCP_STATE_INIT;
    g_dhcp_retry_cnt = 0U;
    DHCP_Set_State(DHCP_INIT);

    g_w5500_recovering = 0U;

    getSIPR(ip);
    printf("[DHCP-WDG] W5500 reset, restart DHCP, IP=%u.%u.%u.%u\r\n",
           (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
}

/********************************************************************************************
* 函数名：DHCP_Watchdog_Task
* 描  述：DHCP 看门狗 — 开机 40s 后开始检测；50s 仍无 IP 则硬复位；之后每 10s 检测
* 判  定：W5500_Is_IP_Valid() 为 true 视为已获取 IP（非 0.0.0.0 / 255.255.255.255）
* 注  意：链路 DOWN 时不复位（由 DHCP_Check_Link_Status 处理）
********************************************************************************************/
void DHCP_Watchdog_Task(void)
{
    static uint32_t s_last_recover_tick = 0U;
    uint32_t now = HAL_GetTick();

    if(!ConfigMsg.dhcp)
        return;

    if(g_w5500_recovering)
        return;

    if(now < DHCP_WDG_ARM_MS)
        return;

    if(!(getPHYCFGR() & LINK))
        return;

    if(W5500_Is_IP_Valid())
        return;

    if(s_last_recover_tick == 0U)
    {
        if(now < (DHCP_WDG_ARM_MS + DHCP_WDG_FIRST_WAIT_MS))
            return;
    }
    else if((now - s_last_recover_tick) < DHCP_WDG_PERIOD_MS)
    {
        return;
    }

    s_last_recover_tick = now;
    W5500_DHCP_Network_Recover();
}

/* ==================== 函数实现 ==================== */

/********************************************************************************************
* 函数名：W5500_DHCP_Init
* 描  述：W5500 DHCP 初始化（配置为动态 IP）
*         使用 WIZnet 官方 DHCP 库实现真正的 DHCP 客户端功能
* 注  意：需在 set_w5500_default() 之后调用
********************************************************************************************/
void W5500_DHCP_Init(void)
{
    uint8_t ip[4];
    
    /* 配置 DHCP 模式 */
    ConfigMsg.dhcp = 1;  // 1=动态 IP, 0=静态 IP
    
    /* 初始化 WIZnet DHCP 库 */
    if(DHCP_Init(DHCP_SOCKET) == DHCP_OK)
    {
        printf("[DHCP] WIZnet DHCP client ready\r\n");
        printf("[DHCP] requesting dynamic IP...\r\n");
        
        /* DHCP 未获取前使用 0.0.0.0，避免误用静态 fallback 地址 */
        uint8_t default_ip[4] = DEFAULT_IP_ADDR;
        setSIPR(default_ip);
        getSIPR(ip);
        // printf("[DHCP] 当前 IP: %d.%d.%d.%d (DHCP 获取前使用)\r\n", 
        //        ip[0], ip[1], ip[2], ip[3]);
        
        g_dhcp_state = DHCP_STATE_INIT;
    }
    else
    {
        printf("[DHCP] init failed, will retry\r\n");
        g_dhcp_state = DHCP_STATE_INIT;
    }
    
    g_dhcp_retry_cnt = 0;
}

static uint8_t s_phy_polled_this_loop = 0u;

uint8_t W5500_PhyLink_DebouncedPoll(bool *rising, bool *falling)
{
    static uint8_t stable_up = 0u;
    static uint8_t down_pending = 0u;
    static uint32_t down_since_ms = 0u;
    static uint8_t edge_up = 0u;
    static uint8_t edge_down = 0u;
    uint8_t raw_up;

    if (!s_phy_polled_this_loop) {
        uint32_t now = HAL_GetTick();

        s_phy_polled_this_loop = 1u;
        edge_up = 0u;
        edge_down = 0u;
        raw_up = ((getPHYCFGR() & LINK) != 0u) ? 1u : 0u;

        if (raw_up) {
            down_pending = 0u;
            if (!stable_up) {
                stable_up = 1u;
                edge_up = 1u;
            }
        } else if (stable_up) {
            if (!down_pending) {
                down_pending = 1u;
                down_since_ms = now;
            } else if ((now - down_since_ms) >= PHY_LINK_DOWN_DEBOUNCE_MS) {
                stable_up = 0u;
                down_pending = 0u;
                edge_down = 1u;
            }
        }
    }

    if (rising != NULL) {
        *rising = (edge_up != 0u);
    }
    if (falling != NULL) {
        *falling = (edge_down != 0u);
    }
    return stable_up;
}

void W5500_PhyLink_DebouncedLoopBegin(void)
{
    s_phy_polled_this_loop = 0u;
}

/********************************************************************************************
* 函数名：DHCP_Check_Link_Status
* 描  述：检查网络链路状态（支持链路断开后自动重连）
* 注  意：link down 需持续 PHY_LINK_DOWN_DEBOUNCE_MS 才认定；link up 仍立即处理
********************************************************************************************/
static void DHCP_Check_Link_Status(void)
{
    bool link_up = false;
    bool link_down = false;

    (void)W5500_PhyLink_DebouncedPoll(&link_up, &link_down);

    if (link_up) {
        printf("[DHCP] link up, trigger DHCP\r\n");
        g_dhcp_state = DHCP_STATE_INIT;
        g_dhcp_retry_cnt = 0;
        DHCP_Set_State(DHCP_INIT);
        return;
    }

    if (link_down) {
        printf("[DHCP] link down (debounced %ums)\r\n", (unsigned)PHY_LINK_DOWN_DEBOUNCE_MS);
        {
            uint8_t zero_ip[4] = {0, 0, 0, 0};
            setSIPR(zero_ip);
        }
        g_dhcp_state = DHCP_STATE_TIMEOUT;
    }
}

/********************************************************************************************
* 函数名：W5500_Get_IP
* 描  述：获取当前 IP 地址
********************************************************************************************/
void W5500_Get_IP(uint8_t *ip_addr)
{
    if(ip_addr == NULL)
        return;
    
    getSIPR(ip_addr);
    
    /* 更新缓存 - 按字节拷贝 */
//    for(uint8_t i = 0; i < 4; i++)
//    {
//        g_current_ip[i] = ip_addr[i];
//    }
}

/********************************************************************************************
* 函数名：W5500_Is_IP_Valid
* 描  述：检查 IP 是否有效（非 0.0.0.0）
********************************************************************************************/
bool W5500_Is_IP_Valid(void)
{
    uint8_t ip[4];
    getSIPR(ip);
    
    /* 检查是否为 0.0.0.0 */
    if((ip[0] == 0) && (ip[1] == 0) && (ip[2] == 0) && (ip[3] == 0))
        return false;
    
    /* 检查是否为 255.255.255.255 */
    if((ip[0] == 255) && (ip[1] == 255) && (ip[2] == 255) && (ip[3] == 255))
        return false;
    
    return true;
}

/********************************************************************************************
* 函数名：UDP_Broadcast_Init
* 描  述：UDP 广播初始化
********************************************************************************************/
void UDP_Broadcast_Init(void)
{
    uint8_t ret;
    
    /* 关闭 Socket 2 */
    close(UDP_BROADCAST_SOCKET);
    
    /* UDP 发往组播 236.2.3.6:2468；需要设置 Sn_MR_MULTI 标志 (0x80) 以支持组播 */
    /* Sn_MR_MULTI = 0x80 启用UDP组播支持 */
    ret = socket(UDP_BROADCAST_SOCKET, Sn_MR_UDP | Sn_MR_MULTI, UDP_DISCOVER_MULTICAST_PORT, 0);
    
    if(ret == 1)
    {
        /* SIPR 有效后打开 Socket，并立即写入组播目标，避免 Sn_DIPR 仍为 0.0.0.0 */
        UDP_Set_Destination(UDP_BROADCAST_SOCKET, g_discover_mcast_ip, UDP_DISCOVER_MULTICAST_PORT);
        printf("[UDP] mcast socket %d -> %u.%u.%u.%u:%u\r\n",
                UDP_BROADCAST_SOCKET,
                (unsigned)g_discover_mcast_ip[0], (unsigned)g_discover_mcast_ip[1],
                (unsigned)g_discover_mcast_ip[2], (unsigned)g_discover_mcast_ip[3],
                (unsigned)UDP_DISCOVER_MULTICAST_PORT);
        g_broadcast_enabled = 1;
    }
    else
    {
        printf("[UDP] mcast socket init failed\r\n");
        g_broadcast_enabled = 0;
    }
}

/********************************************************************************************
* 函数名：UDP_Broadcast_Send
* 描  述：发送 UDP 广播数据
********************************************************************************************/
uint16_t UDP_Broadcast_Send(const uint8_t *data, uint16_t len)
{
    uint16_t sent_m = 0;

    if (!g_broadcast_enabled || data == NULL || len == 0)
        return 0;

    /* 组播：236.2.3.6:2468（配置 DIPR + 组播 MAC 后再发） */
    UDP_Set_Destination(UDP_BROADCAST_SOCKET, g_discover_mcast_ip, UDP_DISCOVER_MULTICAST_PORT);
    sent_m = sendto(UDP_BROADCAST_SOCKET, data, len,
                    (uint8_t *)g_discover_mcast_ip, UDP_DISCOVER_MULTICAST_PORT);

    return sent_m;
}

/********************************************************************************************
* 函数名：Broadcast_Fill_Data
* 描  述：按 tmpdocs 协议填充「网口组播发现」一行（逗号分隔 ASCII）
* 字段顺序：产品型号,产品序列号,ip地址,控制端口,数据流端口,协议地址,协议类型,预留
********************************************************************************************/
uint16_t Broadcast_Fill_Data(uint8_t *buf)
{
    uint8_t ip[4];
    int n;
    char sn_safe[DEVICE_CFG_SN_LEN + 1U];
    size_t i;

    const char *sn;
    const char *model;
    size_t sn_len;
    size_t model_len;

    if (buf == NULL)
        return 0;

    W5500_Get_IP(ip);

    sn = DeviceConfig_GetSn();
    model = DeviceConfig_GetProductModel();
    if (sn == NULL) {
        sn = NEIJI_DEVICE_SN;
    }
    if (model == NULL) {
        model = DEVICE_PRODUCT_MODEL;
    }

    sn_len = strlen(sn);
    if (sn_len > (size_t)DEVICE_CFG_SN_LEN) {
        sn_len = (size_t)DEVICE_CFG_SN_LEN;
    }
    for (i = 0U; i < sn_len; i++)
        sn_safe[i] = sn[i];
    sn_safe[sn_len] = '\0';

    model_len = strlen(model);
    if (model_len > 16U) {
        model_len = 16U;
    }

    n = snprintf((char *)buf, (size_t)UDP_DISCOVER_PAYLOAD_MAX,
                 "%.*s,%s,%u.%u.%u.%u,%u,%u,%u,%u,%u",
                 (int)model_len, model,
                 sn_safe,
                 (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
                 (unsigned)SETTING_SOCKET_PORT,
                 (unsigned)DATA_UPLOAD_SOCKET_PORT,
                 (unsigned)Fsy_Dispatch_GetDeviceAddr(),
                 (unsigned)DEVICE_PROTOCOL_TYPE_CODE,
                 (unsigned)DEVICE_UDP_DISCOVER_RESERVED);

    if (n <= 0 || n >= (int)UDP_DISCOVER_PAYLOAD_MAX)
        return 0;

    return (uint16_t)n;
}

/********************************************************************************************
* 函数名：UDP_Broadcast_Task
* 描  述：UDP 广播任务（需在 FreeRTOS 任务中调用）
********************************************************************************************/
void UDP_Broadcast_Task(void)
{
    static uint8_t broadcast_buf[UDP_DISCOVER_PAYLOAD_MAX];
    uint32_t current_tick = HAL_GetTick();
    uint8_t ip[4];

    if(g_w5500_recovering)
        return;

    getSIPR(ip);

    /* IP 无效：关闭组播 Socket，等待 DHCP */
    if(!W5500_Is_IP_Valid())
    {
        if((s_udp_bound_ip[0] | s_udp_bound_ip[1] | s_udp_bound_ip[2] | s_udp_bound_ip[3]) != 0U)
        {
            s_udp_bound_ip[0] = 0U;
            s_udp_bound_ip[1] = 0U;
            s_udp_bound_ip[2] = 0U;
            s_udp_bound_ip[3] = 0U;
            g_broadcast_enabled = 0;
            close(UDP_BROADCAST_SOCKET);
        }

        if((current_tick - g_broadcast_last_tick) >= 5000U)
        {
            printf("[UDP] invalid IP (%u.%u.%u.%u), wait for DHCP...\r\n",
                   (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
            g_broadcast_last_tick = current_tick;
        }
        return;
    }

    /* IP 有效且与上次绑定不同：重新打开组播 Socket（DHCP 首次分配 / 续租变更） */
    if((ip[0] != s_udp_bound_ip[0]) || (ip[1] != s_udp_bound_ip[1]) ||
        (ip[2] != s_udp_bound_ip[2]) || (ip[3] != s_udp_bound_ip[3]))
    {
        printf("[UDP] IP active (%u.%u.%u.%u), rebind mcast socket\r\n",
                (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
        s_udp_bound_ip[0] = ip[0];
        s_udp_bound_ip[1] = ip[1];
        s_udp_bound_ip[2] = ip[2];
        s_udp_bound_ip[3] = ip[3];
        g_broadcast_last_tick = current_tick;
        UDP_Broadcast_Init();
        Net_Tcp_RebindOnIpChange();
    }

    if(!g_broadcast_enabled)
        return;

    /* 检查时间间隔 */
    if((current_tick - g_broadcast_last_tick) >= UDP_BROADCAST_INTERVAL_MS)
    {
        g_broadcast_last_tick = current_tick;

        /* 填充广播数据 */
        uint16_t len = Broadcast_Fill_Data(broadcast_buf);

        if(len > 0)
        {
            /* 发送广播 */
            uint16_t sent = UDP_Broadcast_Send(broadcast_buf, len);

            if(sent <= 0)
            {
                printf("[UDP] discover broadcast failed\r\n");
            }
            // else
            // {
            //     printf("[UDP] 广播设备信息：源 IP %u.%u.%u.%u -> 236.2.3.6:%u\r\n",
            //             (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3],
            //             (unsigned)UDP_DISCOVER_MULTICAST_PORT);
            // }
        }
    }
}

/********************************************************************************************
* 函数名：DHCP_Client_Task
* 描  述：DHCP 客户端任务（周期性调用，处理 DHCP 状态机）
*         在 W5500_Task 中调用，处理 DHCP 获取和重连
* 注  意：链路断开时会进入 TIMEOUT 状态，链路恢复后自动重连
********************************************************************************************/
DHCP_State_t DHCP_Client_Task(void)
{
    int8_t dhcp_ret;
    
    /* 如果未启用 DHCP，直接返回 */
    if(!ConfigMsg.dhcp)
    {
        g_dhcp_state = DHCP_STATE_OFF;
        return g_dhcp_state;
    }

    if(g_w5500_recovering)
        return g_dhcp_state;
    
    /* 检查链路状态（会更新 g_dhcp_state） */
    DHCP_Check_Link_Status();
    
    /* 如果链路断开，不执行 DHCP，等待链路恢复 */
    if(g_dhcp_state == DHCP_STATE_TIMEOUT)
    {
        return g_dhcp_state;
    }
    
    /* 运行 WIZnet DHCP 库 */
    dhcp_ret = DHCP_Run();
    
    if(dhcp_ret == DHCP_OK)
    {
        uint8_t ip[4];

        getSIPR(ip);

        /* DHCP 成功获取 IP（仅在首次进入 BOUND 或 IP 变更时重建 TCP/UDP） */
        if(g_dhcp_state != DHCP_STATE_BOUND ||
           (ip[0] != s_tcp_bound_ip[0]) || (ip[1] != s_tcp_bound_ip[1]) ||
           (ip[2] != s_tcp_bound_ip[2]) || (ip[3] != s_tcp_bound_ip[3]))
        {
            if(g_dhcp_state != DHCP_STATE_BOUND)
            {
                printf("[DHCP] bound OK\r\n");
                printf("[DHCP] IP %u.%u.%u.%u, rebind mcast socket\r\n",
                       (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
            }
            else
            {
                printf("[DHCP] IP changed %u.%u.%u.%u -> %u.%u.%u.%u, rebuild sockets\r\n",
                       (unsigned)s_tcp_bound_ip[0], (unsigned)s_tcp_bound_ip[1],
                       (unsigned)s_tcp_bound_ip[2], (unsigned)s_tcp_bound_ip[3],
                       (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
            }

            s_tcp_bound_ip[0] = ip[0];
            s_tcp_bound_ip[1] = ip[1];
            s_tcp_bound_ip[2] = ip[2];
            s_tcp_bound_ip[3] = ip[3];
            UDP_Broadcast_Init();
            Net_Tcp_RebindOnIpChange();
        }
        g_dhcp_state = DHCP_STATE_BOUND;
        g_dhcp_retry_cnt = 0;
    }
    else if(dhcp_ret == DHCP_TIMEOUT)
    {
        /* DHCP 超时 */
        g_dhcp_retry_cnt++;
        
        if(g_dhcp_retry_cnt >= DHCP_MAX_RETRY)
        {
            printf("[DHCP] timeout, retry from start (%d/%d)\r\n", 
                   g_dhcp_retry_cnt, DHCP_MAX_RETRY);
            /* 重置重试计数器，继续尝试 DHCP */
            g_dhcp_retry_cnt = 0;
            g_dhcp_state = DHCP_STATE_INIT;
            /* 注意：不调用 DHCP_Stop/Init，避免频繁开关 Socket */
        }
        else
        {
            printf("[DHCP] acquiring address... (retry %d/%d)\r\n", 
                   g_dhcp_retry_cnt, DHCP_MAX_RETRY);
            g_dhcp_state = DHCP_STATE_INIT;
            /* 注意：不调用 DHCP_Stop/Init，避免频繁开关 Socket */
        }
    }
    else if(dhcp_ret == DHCP_RUNNING)
    {
        /* DHCP 正在运行 */
        g_dhcp_state = DHCP_STATE_DISCOVER;
    }
    else
    {
        /* DHCP 失败 */
        g_dhcp_state = DHCP_STATE_FAILED;
        /* 失败后重置，允许重新尝试 */
        g_dhcp_retry_cnt = 0;
    }
    
    return g_dhcp_state;
}

/********************************************************************************************
* 函数名：DHCP_Get_State
* 描  述：获取当前 DHCP 状态
********************************************************************************************/
//DHCP_State_t DHCP_Get_State(void)
//{
//    return g_dhcp_state;
//}

/********************************************************************************************
* 函数名：DHCP_Reset
* 描  述：重置 DHCP 状态（重新获取 IP）
********************************************************************************************/
void DHCP_Reset(void)
{
    printf("[DHCP] reset state\r\n");
    g_dhcp_state = DHCP_STATE_INIT;
    g_dhcp_retry_cnt = 0;
}
