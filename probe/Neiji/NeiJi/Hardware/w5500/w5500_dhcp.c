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

/* 冷上电计时：W5500_Network_OnBoot 刷新，用于宽限期内忽略 PHY 抖动 */
static uint32_t g_net_boot_tick = 0U;

/* 组播连续 send 失败计数 */
static uint8_t g_mcast_send_fail_streak = 0U;

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

static uint8_t UDP_Open_Multicast_Socket(SOCKET s)
{
    close(s);

    /*
     * W5500 multicast mode samples destination IP/port/MAC around OPEN.
     * Configure them before OPEN; sendto() still refreshes them before each SEND.
     */
    UDP_Set_Destination(s, g_discover_mcast_ip, UDP_DISCOVER_MULTICAST_PORT);
    IINCHIP_WRITE(Sn_TTL(s), 1U);
    IINCHIP_WRITE(Sn_MR(s), (uint8_t)(Sn_MR_UDP | Sn_MR_MULTI));
    IINCHIP_WRITE(Sn_PORT0(s), (uint8_t)((UDP_DISCOVER_MULTICAST_PORT & 0xff00u) >> 8));
    IINCHIP_WRITE(Sn_PORT1(s), (uint8_t)(UDP_DISCOVER_MULTICAST_PORT & 0x00ffu));
    IINCHIP_WRITE(Sn_CR(s), Sn_CR_OPEN);
    while (IINCHIP_READ(Sn_CR(s)) != 0U) {
        ;
    }

    return (getSn_SR(s) == SOCK_UDP) ? 1U : 0U;
}

/* ==================== 内部函数声明 ==================== */
static void DHCP_Check_Link_Status(void);
static void W5500_DHCP_Network_Recover(void);
static bool net_in_boot_grace(void);
static bool w5500_sipr_read_stable(uint8_t ip[4]);

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
    static uint32_t s_invalid_since = 0U;
    uint32_t now = HAL_GetTick();

    if(!ConfigMsg.dhcp)
        return;

    if(g_w5500_recovering)
        return;

    if(now < DHCP_WDG_ARM_MS)
        return;

    if(!(getPHYCFGR() & LINK))
        return;

    if(W5500_Is_IP_Valid()) {
        s_invalid_since = 0U;
        return;
    }

    if(s_invalid_since == 0U) {
        s_invalid_since = now;
    } else if((now - s_invalid_since) < 3000U) {
        return;
    }

    if(s_last_recover_tick == 0U)
    {
        if(now < (DHCP_WDG_ARM_MS + DHCP_WDG_FIRST_WAIT_MS))
            return;
    }
    else if((now - s_last_recover_tick) < DHCP_WDG_PERIOD_MS)
    {
        return;
    }

    s_invalid_since = 0U;
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
static uint8_t s_phy_stable_up = 0u;
static uint8_t s_phy_down_pending = 0u;
static uint32_t s_phy_down_since_ms = 0u;
static uint8_t s_phy_edge_up = 0u;
static uint8_t s_phy_edge_down = 0u;

uint8_t W5500_PhyLink_DebouncedPoll(bool *rising, bool *falling)
{
    uint8_t raw_up;

    if (!s_phy_polled_this_loop) {
        uint32_t now = HAL_GetTick();

        s_phy_polled_this_loop = 1u;
        s_phy_edge_up = 0u;
        s_phy_edge_down = 0u;
        raw_up = ((getPHYCFGR() & LINK) != 0u) ? 1u : 0u;

        if (raw_up) {
            s_phy_down_pending = 0u;
            if (!s_phy_stable_up) {
                s_phy_stable_up = 1u;
                s_phy_edge_up = 1u;
            }
        } else if (s_phy_stable_up) {
            if (!s_phy_down_pending) {
                s_phy_down_pending = 1u;
                s_phy_down_since_ms = now;
            } else if ((now - s_phy_down_since_ms) >= PHY_LINK_DOWN_DEBOUNCE_MS) {
                s_phy_stable_up = 0u;
                s_phy_down_pending = 0u;
                s_phy_edge_down = 1u;
            }
        }
    }

    if (rising != NULL) {
        *rising = (s_phy_edge_up != 0u);
    }
    if (falling != NULL) {
        *falling = (s_phy_edge_down != 0u);
    }
    return s_phy_stable_up;
}

void W5500_PhyLink_DebouncedLoopBegin(void)
{
    s_phy_polled_this_loop = 0u;
}

void W5500_PhyLink_ResetDebouncer(void)
{
    s_phy_polled_this_loop = 0u;
    s_phy_stable_up = 0u;
    s_phy_down_pending = 0u;
    s_phy_down_since_ms = 0u;
    s_phy_edge_up = 0u;
    s_phy_edge_down = 0u;
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
        if (g_dhcp_state == DHCP_STATE_BOUND && W5500_Is_IP_Valid() && net_in_boot_grace()) {
            return;
        }
        printf("[DHCP] link up, trigger DHCP\r\n");
        g_dhcp_state = DHCP_STATE_INIT;
        g_dhcp_retry_cnt = 0;
        DHCP_Stop();
        (void)DHCP_Init(DHCP_SOCKET);
        return;
    }

    if (link_down) {
        if (g_dhcp_state == DHCP_STATE_BOUND && W5500_Is_IP_Valid() && net_in_boot_grace()) {
            return;
        }
        printf("[DHCP] link down (debounced %ums)\r\n", (unsigned)PHY_LINK_DOWN_DEBOUNCE_MS);
        {
            uint8_t zero_ip[4] = {0, 0, 0, 0};
            setSIPR(zero_ip);
        }
        g_dhcp_state = DHCP_STATE_TIMEOUT;
    }
}

/********************************************************************************************
* 函数名：W5500_Is_IP_Valid_Buf
* 描  述：检查 IP 是否为可用局域网地址（RFC1918，排除 0/255/环回/组播）
********************************************************************************************/
bool W5500_Is_IP_Valid_Buf(const uint8_t ip[4])
{
    if (ip == NULL) {
        return false;
    }

    if ((ip[0] | ip[1] | ip[2] | ip[3]) == 0U) {
        return false;
    }

    if ((ip[0] == 255U) && (ip[1] == 255U) && (ip[2] == 255U) && (ip[3] == 255U)) {
        return false;
    }

    if (ip[0] == 127U || ip[0] >= 224U) {
        return false;
    }

    if (ip[0] == 10U) {
        return true;
    }

    if ((ip[0] == 172U) && (ip[1] >= 16U) && (ip[1] <= 31U)) {
        return true;
    }

    if ((ip[0] == 192U) && (ip[1] == 168U)) {
        return true;
    }

    return false;
}

static bool w5500_sipr_read_stable(uint8_t ip[4])
{
    uint8_t ip_a[4];
    uint8_t ip_b[4];
    uint8_t attempt;

    for (attempt = 0U; attempt < 3U; attempt++) {
        getSIPR(ip_a);
        getSIPR(ip_b);

        if ((ip_a[0] == ip_b[0]) && (ip_a[1] == ip_b[1]) &&
            (ip_a[2] == ip_b[2]) && (ip_a[3] == ip_b[3])) {
            ip[0] = ip_a[0];
            ip[1] = ip_a[1];
            ip[2] = ip_a[2];
            ip[3] = ip_a[3];
            return true;
        }
    }

    return false;
}

static bool net_in_boot_grace(void)
{
    return ((HAL_GetTick() - g_net_boot_tick) < NET_BOOT_GRACE_MS);
}

bool W5500_Is_NetBootGrace(void)
{
    return net_in_boot_grace();
}

void W5500_Get_Active_IP(uint8_t ip[4])
{
    if (ip == NULL) {
        return;
    }

    /* DHCP 模式：仅以 W5500 SIPR 为准，避免 Flash 静态备用地址(如 192.168.2.100)误导组播 */
    if (ConfigMsg.dhcp) {
        if (!w5500_sipr_read_stable(ip)) {
            ip[0] = 0U;
            ip[1] = 0U;
            ip[2] = 0U;
            ip[3] = 0U;
        }
        return;
    }

    if (W5500_Is_IP_Valid_Buf(ConfigMsg.lip)) {
        ip[0] = ConfigMsg.lip[0];
        ip[1] = ConfigMsg.lip[1];
        ip[2] = ConfigMsg.lip[2];
        ip[3] = ConfigMsg.lip[3];
        return;
    }

    if (!w5500_sipr_read_stable(ip)) {
        ip[0] = 0U;
        ip[1] = 0U;
        ip[2] = 0U;
        ip[3] = 0U;
    }
}

/********************************************************************************************
* 函数名：W5500_Is_IP_Valid
* 描  述：检查 W5500 是否已有可用 IP
* 注  意：DHCP 模式下必须已 BOUND 且 SIPR 有效，不能用 Flash 静态备用地址冒充
********************************************************************************************/
bool W5500_Is_IP_Valid(void)
{
    uint8_t ip[4];

    if (ConfigMsg.dhcp) {
        if (g_dhcp_state != DHCP_STATE_BOUND) {
            return false;
        }
        if (!w5500_sipr_read_stable(ip)) {
            return false;
        }
        return W5500_Is_IP_Valid_Buf(ip);
    }

    if (W5500_Is_IP_Valid_Buf(ConfigMsg.lip)) {
        return true;
    }

    if (!w5500_sipr_read_stable(ip)) {
        return false;
    }

    return W5500_Is_IP_Valid_Buf(ip);
}

/********************************************************************************************
* 函数名：W5500_Network_OnBoot
* 描  述：MCU 复位后清网络运行时状态；W5500 不断电时 PHY 不断，需强制重走 DHCP/组播
********************************************************************************************/
void W5500_Network_OnBoot(void)
{
    g_net_boot_tick = HAL_GetTick();
    g_mcast_send_fail_streak = 0U;
    g_broadcast_enabled = 0U;
    g_broadcast_last_tick = 0U;
    s_udp_bound_ip[0] = 0U;
    s_udp_bound_ip[1] = 0U;
    s_udp_bound_ip[2] = 0U;
    s_udp_bound_ip[3] = 0U;
    s_tcp_bound_ip[0] = 0U;
    s_tcp_bound_ip[1] = 0U;
    s_tcp_bound_ip[2] = 0U;
    s_tcp_bound_ip[3] = 0U;
    g_dhcp_state = DHCP_STATE_INIT;
    g_dhcp_retry_cnt = 0U;

    /* 仅清 MCU 侧运行时状态；socket 关闭须在 sysinit 之后（见 set_w5500_network） */
    if (ConfigMsg.dhcp) {
        printf("[net] MCU boot: clear stale IP, restart DHCP\r\n");
    } else {
        printf("[net] MCU boot: static IP, restart mcast/TCP\r\n");
    }
}

void W5500_SyncBoundIp(const uint8_t ip[4])
{
    if (ip == NULL) {
        return;
    }
    s_udp_bound_ip[0] = ip[0];
    s_udp_bound_ip[1] = ip[1];
    s_udp_bound_ip[2] = ip[2];
    s_udp_bound_ip[3] = ip[3];
    s_tcp_bound_ip[0] = ip[0];
    s_tcp_bound_ip[1] = ip[1];
    s_tcp_bound_ip[2] = ip[2];
    s_tcp_bound_ip[3] = ip[3];
}

/********************************************************************************************
* 函数名：W5500_Get_IP
* 描  述：获取当前 IP 地址
********************************************************************************************/
void W5500_Get_IP(uint8_t *ip_addr)
{
    W5500_Get_Active_IP(ip_addr);
}

/********************************************************************************************
* 函数名：UDP_Broadcast_Init
* 描  述：UDP 广播初始化
********************************************************************************************/
void UDP_Broadcast_Init(void)
{
    uint8_t ret;

    ret = UDP_Open_Multicast_Socket(UDP_BROADCAST_SOCKET);
    
    if(ret == 1)
    {
        /* 再刷新一次，便于串口诊断时寄存器保持目标组播地址 */
        UDP_Set_Destination(UDP_BROADCAST_SOCKET, g_discover_mcast_ip, UDP_DISCOVER_MULTICAST_PORT);
        printf("[UDP] mcast socket %d -> %u.%u.%u.%u:%u\r\n",
                UDP_BROADCAST_SOCKET,
                (unsigned)g_discover_mcast_ip[0], (unsigned)g_discover_mcast_ip[1],
                (unsigned)g_discover_mcast_ip[2], (unsigned)g_discover_mcast_ip[3],
                (unsigned)UDP_DISCOVER_MULTICAST_PORT);
        g_broadcast_enabled = 1;
        g_mcast_send_fail_streak = 0U;
        /* 下次 Task 循环立即发首帧组播 */
        g_broadcast_last_tick = HAL_GetTick() - UDP_BROADCAST_INTERVAL_MS;
        printf("[UDP] mcast init sr=0x%02X enabled=%u\r\n",
               (unsigned)getSn_SR(UDP_BROADCAST_SOCKET),
               (unsigned)g_broadcast_enabled);
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
    static uint32_t s_mcast_recover_tick = 0U;
    static uint32_t s_invalid_ip_since = 0U;
    uint32_t current_tick = HAL_GetTick();
    uint8_t ip[4];

    if(g_w5500_recovering)
        return;

    /*
     * 静态 IP：以 ConfigMsg.lip 为准，不依赖 SIPR 偶发读失败。
     * DHCP：Get_Active_IP 内部需 SIPR 连续两次一致才认为有效。
     */
    W5500_Get_Active_IP(ip);

    /* IP 无效：关闭组播 Socket，等待 DHCP */
    if(!W5500_Is_IP_Valid())
    {
        if (s_invalid_ip_since == 0U) {
            s_invalid_ip_since = current_tick;
        }
        if ((current_tick - s_invalid_ip_since) < 1000U) {
            return;
        }

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
    s_invalid_ip_since = 0U;

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
    {
        if((current_tick - s_mcast_recover_tick) >= UDP_MCAST_RECOVER_MS)
        {
            s_mcast_recover_tick = current_tick;
            printf("[UDP] mcast disabled but IP valid, retry init\r\n");
            UDP_Broadcast_Init();
        }
        return;
    }

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
                g_mcast_send_fail_streak++;
                printf("[UDP] tx fail sr=0x%02X len=%u streak=%u\r\n",
                       (unsigned)getSn_SR(UDP_BROADCAST_SOCKET),
                       (unsigned)len,
                       (unsigned)g_mcast_send_fail_streak);
                if (g_mcast_send_fail_streak >= UDP_MCAST_SEND_FAIL_MAX) {
                    printf("[UDP] discover broadcast failed x%u, rebuild socket\r\n",
                           (unsigned)g_mcast_send_fail_streak);
                    g_broadcast_enabled = 0U;
                    g_mcast_send_fail_streak = 0U;
                }
            } else {
                g_mcast_send_fail_streak = 0U;
            }
        } else {
            static uint32_t s_fill_fail_log_tick = 0U;

            if ((current_tick - s_fill_fail_log_tick) >= 5000U) {
                s_fill_fail_log_tick = current_tick;
                printf("[UDP] Broadcast_Fill_Data returned 0\r\n");
            }
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
            s_udp_bound_ip[0] = ip[0];
            s_udp_bound_ip[1] = ip[1];
            s_udp_bound_ip[2] = ip[2];
            s_udp_bound_ip[3] = ip[3];
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
        /* 续租/重试进行中：已 BOUND 时保持 BOUND，避免看门狗误判 */
        if(g_dhcp_state != DHCP_STATE_BOUND) {
            g_dhcp_state = DHCP_STATE_DISCOVER;
        }
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
    if (ConfigMsg.dhcp) {
        (void)DHCP_Init(DHCP_SOCKET);
    }
}
