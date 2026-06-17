/*******************************************************************************
 * WIZnet DHCP Library for W5500
 * 移植自：https://github.com/WIZnet/ioLibrary
 ******************************************************************************/

#include "dhcp.h"
#include "device.h"
#include "config.h"

#include <stdio.h>

/* ==================== 自定义内存操作函数 (避免对齐问题) ==================== */

/* 自定义 memset - 按字节填充 */
static void my_memset(uint8_t *dest, uint8_t val, uint16_t len)
{
    uint16_t i;
    for(i = 0; i < len; i++)
    {
        dest[i] = val;
    }
}

/* 自定义 memcpy - 按字节拷贝 */
static void my_memcpy(uint8_t *dest, const uint8_t *src, uint16_t len)
{
    uint16_t i;
    for(i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }
}

/* 自定义 memcmp - 按字节比较 */
static int8_t my_memcmp(const uint8_t *s1, const uint8_t *s2, uint16_t len)
{
    uint16_t i;
    for(i = 0; i < len; i++)
    {
        if(s1[i] != s2[i])
            return (s1[i] > s2[i]) ? 1 : -1;
    }
    return 0;
}

/* ==================== 全局变量 ==================== */
static uint8_t dhcp_socket = 0xFF;
static uint8_t dhcp_state = DHCP_INIT;
static uint8_t dhcp_retry = 0;
static uint32_t dhcp_last_tick = 0;
static DHCP_Info_t dhcp_info = {0};

/* DHCP 消息缓冲 (使用静态分配，避免栈溢出) */
static uint8_t dhcp_tx_buf[548] __attribute__((aligned(4)));  // 发送缓冲
static uint8_t dhcp_rx_buf[548] __attribute__((aligned(4)));  // 接收缓冲

/* 外部 MAC 地址 */
extern CONFIG_MSG ConfigMsg;

/* 随机数生成 (用于 XID) */
static uint32_t DHCP_Generate_XID(void)
{
    static uint32_t xid = 0;
    if(xid == 0)
    {
        xid = HAL_GetTick();
    }
    else
    {
        xid++;
    }
    return xid;
}

/* 设置 DHCP 选项 */
static uint16_t DHCP_Set_Options(uint8_t *opt, uint8_t msg_type)
{
    uint8_t *p = opt;
    
    /* Magic Cookie (offset 236-239) */
    *p++ = 99;
    *p++ = 130;
    *p++ = 83;
    *p++ = 99;
    
    /* 消息类型 */
    *p++ = DHCP_OPT_MSG_TYPE;
    *p++ = 1;
    *p++ = msg_type;
    
    if(msg_type == DHCP_DISCOVER_MSG || msg_type == DHCP_REQUEST_MSG)
    {
        /* 请求的参数 */
        *p++ = DHCP_OPT_PARAM_REQUEST;
        *p++ = 4;
        *p++ = DHCP_OPT_SUBNET_MASK;
        *p++ = DHCP_OPT_ROUTER;
        *p++ = DHCP_OPT_DNS;
        *p++ = DHCP_OPT_IP_LEASE_TIME;
    }
    
    if(msg_type == DHCP_REQUEST_MSG)
    {
        /* 请求的 IP */
        *p++ = DHCP_OPT_REQUEST_IP;
        *p++ = 4;
        my_memcpy(p, dhcp_info.ip, 4);
        p += 4;
        
        /* DHCP 服务器 ID */
        *p++ = DHCP_OPT_SERVER_ID;
        *p++ = 4;
        my_memcpy(p, dhcp_info.dhcp_server, 4);
        p += 4;
    }
    
    /* 结束标志 */
    *p++ = DHCP_OPT_END;
    
    /* 填充到最小长度 (DHCP 要求至少 300 字节) */
    while((p - opt) < 64)  // 64 字节最小选项长度
    {
        *p++ = DHCP_OPT_PAD;
    }
    
    /* 返回总长度 (从 Magic Cookie 开始计算) */
    return (uint16_t)(p - opt);
}

/* 发送 DHCP DISCOVER */
static void DHCP_Send_Discover(void)
{
    uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    uint8_t mac_copy[6];
    uint16_t msg_len;
    
    /* 清空整个缓冲区 */
    my_memset(dhcp_tx_buf, 0, sizeof(dhcp_tx_buf));
    
    /* 拷贝 MAC 地址到本地 */
    my_memcpy(mac_copy, ConfigMsg.mac, 6);
    
    /* === DHCP 固定头部 (offset 0-235) === */
    
    /* op, htype, hlen, hops */
    dhcp_tx_buf[0] = 1;   // BOOTREQUEST
    dhcp_tx_buf[1] = 1;   // Ethernet
    dhcp_tx_buf[2] = 6;   // MAC address length
    dhcp_tx_buf[3] = 0;   // hops
    
    /* XID (4 字节，大端) — 必须写入 dhcp_info，供 Parse_Response 校验 OFFER/ACK */
    uint32_t xid = DHCP_Generate_XID();
    dhcp_info.xid = xid;
    dhcp_tx_buf[4] = (xid >> 24) & 0xFF;
    dhcp_tx_buf[5] = (xid >> 16) & 0xFF;
    dhcp_tx_buf[6] = (xid >> 8) & 0xFF;
    dhcp_tx_buf[7] = xid & 0xFF;
    
    /* secs, flags：无 IP 的客户端应置 BROADCAST，否则部分交换机/服务器不转发单播 OFFER */
    dhcp_tx_buf[8] = 0;
    dhcp_tx_buf[9] = 0;
    dhcp_tx_buf[10] = 0x80;
    dhcp_tx_buf[11] = 0x00;
    
    /* ciaddr (offset 12-15) - 客户端 IP，初始为 0 */
    dhcp_tx_buf[12] = 0;
    dhcp_tx_buf[13] = 0;
    dhcp_tx_buf[14] = 0;
    dhcp_tx_buf[15] = 0;
    
    /* yiaddr (offset 16-19) - 分配的 IP，初始为 0 */
    dhcp_tx_buf[16] = 0;
    dhcp_tx_buf[17] = 0;
    dhcp_tx_buf[18] = 0;
    dhcp_tx_buf[19] = 0;
    
    /* siaddr (offset 20-23) - 服务器 IP，初始为 0 */
    dhcp_tx_buf[20] = 0;
    dhcp_tx_buf[21] = 0;
    dhcp_tx_buf[22] = 0;
    dhcp_tx_buf[23] = 0;
    
    /* giaddr (offset 24-27) - 网关 IP，初始为 0 */
    dhcp_tx_buf[24] = 0;
    dhcp_tx_buf[25] = 0;
    dhcp_tx_buf[26] = 0;
    dhcp_tx_buf[27] = 0;
    
    /* chaddr (offset 28-43) - 客户端 MAC 地址，16 字节 */
    for(uint8_t i = 0; i < 6; i++)
    {
        dhcp_tx_buf[28 + i] = mac_copy[i];
    }
    /* 剩余 10 字节补 0 (已 memset 为 0) */
    
    /* sname (offset 44-107) - 64 字节，补 0 */
    /* file (offset 108-235) - 128 字节，补 0 */
    
    /* === DHCP 选项 (从 offset 236 开始) === */
    uint16_t opt_len = DHCP_Set_Options(&dhcp_tx_buf[236], DHCP_DISCOVER_MSG);
    
    /* 总长度 = 236 (固定头部) + 选项长度 */
    msg_len = 236 + opt_len;
    
    /* 发送 */
    sendto(dhcp_socket, dhcp_tx_buf, msg_len, broadcast_ip, DHCP_SERVER_PORT);
    
    printf("[DHCP] 发送 DISCOVER (len=%d)\r\n", msg_len);
}

/* 发送 DHCP REQUEST */
static void DHCP_Send_Request(void)
{
    uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    uint8_t mac_copy[6];
    uint16_t msg_len;
    
    /* 清空整个缓冲区 */
    my_memset(dhcp_tx_buf, 0, sizeof(dhcp_tx_buf));
    
    /* 拷贝 MAC 地址到本地 */
    my_memcpy(mac_copy, ConfigMsg.mac, 6);
    
    /* === DHCP 固定头部 (offset 0-235) === */
    
    /* op, htype, hlen, hops */
    dhcp_tx_buf[0] = 1;   // BOOTREQUEST
    dhcp_tx_buf[1] = 1;   // Ethernet
    dhcp_tx_buf[2] = 6;   // MAC address length
    dhcp_tx_buf[3] = 0;   // hops
    
    /* XID (4 字节，大端) */
    uint32_t xid = dhcp_info.xid;
    dhcp_tx_buf[4] = (xid >> 24) & 0xFF;
    dhcp_tx_buf[5] = (xid >> 16) & 0xFF;
    dhcp_tx_buf[6] = (xid >> 8) & 0xFF;
    dhcp_tx_buf[7] = xid & 0xFF;
    
    /* secs, flags */
    dhcp_tx_buf[8] = 0;
    dhcp_tx_buf[9] = 0;
    dhcp_tx_buf[10] = 0x80;
    dhcp_tx_buf[11] = 0x00;
    
    /* ciaddr (offset 12-15) - 客户端 IP，初始为 0 */
    dhcp_tx_buf[12] = 0;
    dhcp_tx_buf[13] = 0;
    dhcp_tx_buf[14] = 0;
    dhcp_tx_buf[15] = 0;
    
    /* yiaddr (offset 16-19) - 分配的 IP，初始为 0 */
    dhcp_tx_buf[16] = 0;
    dhcp_tx_buf[17] = 0;
    dhcp_tx_buf[18] = 0;
    dhcp_tx_buf[19] = 0;
    
    /* siaddr (offset 20-23) - 服务器 IP，初始为 0 */
    dhcp_tx_buf[20] = 0;
    dhcp_tx_buf[21] = 0;
    dhcp_tx_buf[22] = 0;
    dhcp_tx_buf[23] = 0;
    
    /* giaddr (offset 24-27) - 网关 IP，初始为 0 */
    dhcp_tx_buf[24] = 0;
    dhcp_tx_buf[25] = 0;
    dhcp_tx_buf[26] = 0;
    dhcp_tx_buf[27] = 0;
    
    /* chaddr (offset 28-43) - 客户端 MAC 地址，16 字节 */
    for(uint8_t i = 0; i < 6; i++)
    {
        dhcp_tx_buf[28 + i] = mac_copy[i];
    }
    /* 剩余 10 字节补 0 (已 memset 为 0) */
    
    /* sname (offset 44-107) - 64 字节，补 0 */
    /* file (offset 108-235) - 128 字节，补 0 */
    
    /* === DHCP 选项 (从 offset 236 开始) === */
    uint16_t opt_len = DHCP_Set_Options(&dhcp_tx_buf[236], DHCP_REQUEST_MSG);
    
    /* 总长度 = 236 (固定头部) + 选项长度 */
    msg_len = 236 + opt_len;
    
    /* 发送 */
    sendto(dhcp_socket, dhcp_tx_buf, msg_len, broadcast_ip, DHCP_SERVER_PORT);
    
    printf("[DHCP] 发送 REQUEST (len=%d)\r\n", msg_len);
}

/* 解析 DHCP OFFER/ACK */
static int8_t DHCP_Parse_Response(uint8_t *buf, uint16_t len)
{
    uint8_t *opt;
    uint8_t *opt_end;
    uint8_t msg_type = 0;
    uint8_t mac_buf[6];
    uint8_t ip_buf[4];
    
    /* 空指针检查 */
    if(buf == NULL)
        return DHCP_FAIL;
    
    /* 长度检查 - 至少需要 236 字节固定头部 + 4 字节 Magic Cookie */
    if(len < 240)
    {
        printf("[DHCP] 响应长度不足：%d < 240\r\n", len);
        return DHCP_FAIL;
    }
    
    /* 检查 MAC (偏移 28,6 字节) - 使用自定义函数 */
    my_memcpy(mac_buf, &buf[28], 6);
    if(my_memcmp(mac_buf, ConfigMsg.mac, 6) != 0)
        return DHCP_FAIL;
    
    /* 获取 XID (偏移 4,4 字节，大端) */
    uint32_t xid = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | 
                   ((uint32_t)buf[6] << 8) | buf[7];
    if(xid != dhcp_info.xid)
        return DHCP_FAIL;
    
    /* 获取选项起始位置 (偏移 236 - Magic Cookie 位置) */
    opt = &buf[236];
    opt_end = &buf[len];  // 到缓冲区末尾
    
    /* 跳过 Magic Cookie */
    if(opt + 4 > opt_end)
    {
        printf("[DHCP] Magic Cookie 越界\r\n");
        return DHCP_FAIL;
    }
    if(opt[0] != 99 || opt[1] != 130 || opt[2] != 83 || opt[3] != 99)
    {
        printf("[DHCP] Magic Cookie 错误：0x%02x 0x%02x 0x%02x 0x%02x\r\n", 
               opt[0], opt[1], opt[2], opt[3]);
        return DHCP_FAIL;
    }
    opt += 4;
    
    /* 解析选项 (带边界检查) */
    while(opt < opt_end && *opt != DHCP_OPT_END)
    {
        if(*opt == DHCP_OPT_PAD)
        {
            opt++;
            continue;
        }
        
        uint8_t code = *opt++;
        
        /* 边界检查：确保有长度字节 */
        if(opt >= opt_end)
            break;
        
        uint8_t opt_len = *opt++;
        
        /* 边界检查：确保选项数据不越界 */
        if(opt + opt_len > opt_end)
            break;
        
        if(code == DHCP_OPT_MSG_TYPE && opt_len >= 1)
        {
            msg_type = opt[0];
        }
        else if(code == DHCP_OPT_SUBNET_MASK && opt_len == 4)
        {
            my_memcpy(dhcp_info.subnet, opt, 4);
        }
        else if(code == DHCP_OPT_ROUTER && opt_len >= 4)
        {
            my_memcpy(dhcp_info.gateway, opt, 4);
        }
        else if(code == DHCP_OPT_DNS && opt_len >= 4)
        {
            my_memcpy(dhcp_info.dns, opt, 4);
        }
        else if(code == DHCP_OPT_SERVER_ID && opt_len == 4)
        {
            my_memcpy(dhcp_info.dhcp_server, opt, 4);
        }
        else if(code == DHCP_OPT_IP_LEASE_TIME && opt_len == 4)
        {
            dhcp_info.lease_time = ((uint32_t)opt[0] << 24) | 
                                   ((uint32_t)opt[1] << 16) | 
                                   ((uint32_t)opt[2] << 8) | 
                                   opt[3];
        }
        
        opt += opt_len;
    }
    
    /* 检查消息类型 */
    if(msg_type != DHCP_OFFER_MSG && msg_type != DHCP_ACK_MSG)
    {
        printf("[DHCP] 未知消息类型：0x%02x\r\n", msg_type);
        return DHCP_FAIL;
    }
    
    /* 获取分配的 IP (偏移 16,4 字节) - 使用自定义函数 */
    my_memcpy(ip_buf, &buf[16], 4);
    my_memcpy(dhcp_info.ip, ip_buf, 4);
    
    return DHCP_OK;
}

/* 应用 DHCP 分配的 IP */
static void DHCP_Apply_IP(void)
{
    printf("[DHCP] 应用 IP: %d.%d.%d.%d\r\n", 
           dhcp_info.ip[0], dhcp_info.ip[1], 
           dhcp_info.ip[2], dhcp_info.ip[3]);
    printf("[DHCP] 子网掩码：%d.%d.%d.%d\r\n", 
           dhcp_info.subnet[0], dhcp_info.subnet[1], 
           dhcp_info.subnet[2], dhcp_info.subnet[3]);
    printf("[DHCP] 网关：%d.%d.%d.%d\r\n", 
           dhcp_info.gateway[0], dhcp_info.gateway[1], 
           dhcp_info.gateway[2], dhcp_info.gateway[3]);
    printf("[DHCP] DNS: %d.%d.%d.%d\r\n", 
           dhcp_info.dns[0], dhcp_info.dns[1], 
           dhcp_info.dns[2], dhcp_info.dns[3]);
    printf("[DHCP] 租约时间：%lu 秒\r\n", dhcp_info.lease_time);
    
    /* 设置 W5500 IP */
    setSIPR(dhcp_info.ip);
    setSUBR(dhcp_info.subnet);
    setGAR(dhcp_info.gateway);

    /* 同步 ConfigMsg，供发现报文与其它模块读取 */
    ConfigMsg.lip[0] = dhcp_info.ip[0];
    ConfigMsg.lip[1] = dhcp_info.ip[1];
    ConfigMsg.lip[2] = dhcp_info.ip[2];
    ConfigMsg.lip[3] = dhcp_info.ip[3];
    ConfigMsg.sub[0] = dhcp_info.subnet[0];
    ConfigMsg.sub[1] = dhcp_info.subnet[1];
    ConfigMsg.sub[2] = dhcp_info.subnet[2];
    ConfigMsg.sub[3] = dhcp_info.subnet[3];
    ConfigMsg.gw[0] = dhcp_info.gateway[0];
    ConfigMsg.gw[1] = dhcp_info.gateway[1];
    ConfigMsg.gw[2] = dhcp_info.gateway[2];
    ConfigMsg.gw[3] = dhcp_info.gateway[3];
}

/*******************************************************************************
 * 公共函数实现
 ******************************************************************************/

int8_t DHCP_Init(uint8_t socket_num)
{
    uint8_t ret;
    
    dhcp_socket = socket_num;
    
    /* 关闭 Socket */
    close(dhcp_socket);
    
    /* 打开 UDP Socket */
    ret = socket(dhcp_socket, Sn_MR_UDP, DHCP_CLIENT_PORT, 0);
    if(ret != 1)
    {
        printf("[DHCP] Socket 打开失败\r\n");
        return DHCP_FAIL;
    }
    
    // printf("[DHCP] Socket %d 已打开 (端口 %d)\r\n", dhcp_socket, DHCP_CLIENT_PORT);
    
    /* 重置状态 */
    dhcp_state = DHCP_INIT;
    dhcp_retry = 0;
    dhcp_last_tick = HAL_GetTick();
    my_memset((uint8_t*)&dhcp_info, 0, sizeof(DHCP_Info_t));
    
    return DHCP_OK;
}

int8_t DHCP_Run(void)
{
    int32_t len;
    uint32_t current_tick = HAL_GetTick();
    
    switch(dhcp_state)
    {
        case DHCP_INIT:
            /* 发送 DISCOVER */
            DHCP_Send_Discover();
            dhcp_state = DHCP_DISCOVER;
            dhcp_last_tick = current_tick;
            dhcp_retry = 0;
            break;
            
        case DHCP_DISCOVER:
            /* 等待 OFFER - 直接使用全局 dhcp_rx_buf */
            len = recvfrom(dhcp_socket, dhcp_rx_buf, sizeof(dhcp_rx_buf), NULL, NULL);
            if(len > 0)
            {
                if(DHCP_Parse_Response(dhcp_rx_buf, len) == DHCP_OK)
                {
                    /* 从偏移 4 读取 XID (大端) */
                    dhcp_info.xid = ((uint32_t)dhcp_rx_buf[4] << 24) | 
                                    ((uint32_t)dhcp_rx_buf[5] << 16) | 
                                    ((uint32_t)dhcp_rx_buf[6] << 8) | 
                                    dhcp_rx_buf[7];
                    dhcp_state = DHCP_REQUEST;
                    printf("[DHCP] 收到 OFFER: %d.%d.%d.%d\r\n", 
                           dhcp_info.ip[0], dhcp_info.ip[1], 
                           dhcp_info.ip[2], dhcp_info.ip[3]);
                }
            }
            
            /* 超时重试 */
            if((current_tick - dhcp_last_tick) >= DHCP_DISCOVER_TIMEOUT)
            {
                dhcp_retry++;
                if(dhcp_retry >= 3)
                {
                    printf("[DHCP] DISCOVER 超时\r\n");
                    dhcp_state = DHCP_INIT;
                    return DHCP_TIMEOUT;
                }
                DHCP_Send_Discover();
                dhcp_last_tick = current_tick;
            }
            break;
            
        case DHCP_REQUEST:
            /* 发送 REQUEST */
            DHCP_Send_Request();
            dhcp_state = DHCP_ACK;
            dhcp_last_tick = current_tick;
            dhcp_retry = 0;
            break;
            
        case DHCP_ACK:
            /* 等待 ACK */
            len = recvfrom(dhcp_socket, dhcp_rx_buf, sizeof(dhcp_rx_buf), NULL, NULL);
            if(len > 0)
            {
                if(DHCP_Parse_Response(dhcp_rx_buf, len) == DHCP_OK)
                {
                    printf("[DHCP] 收到 ACK\r\n");
                    DHCP_Apply_IP();
                    dhcp_state = DHCP_LEASED;
                    return DHCP_OK;
                }
            }
            
            /* 超时重试 */
            if((current_tick - dhcp_last_tick) >= DHCP_REQUEST_TIMEOUT)
            {
                dhcp_retry++;
                if(dhcp_retry >= 3)
                {
                    printf("[DHCP] REQUEST 超时\r\n");
                    dhcp_state = DHCP_INIT;
                    return DHCP_TIMEOUT;
                }
                DHCP_Send_Request();
                dhcp_last_tick = current_tick;
            }
            break;
            
        case DHCP_LEASED:
        {
            /* 租约过半后续租，避免长时间 idle 后 IP 失效 */
            static uint32_t s_lease_tick = 0;
            uint32_t renew_ms;

            if(dhcp_info.lease_time == 0U)
                break;

            renew_ms = (dhcp_info.lease_time / 2U) * 1000U;
            if(renew_ms < 60000U)
                renew_ms = 60000U;

            if(s_lease_tick == 0U)
                s_lease_tick = current_tick;

            if((current_tick - s_lease_tick) >= renew_ms)
            {
                s_lease_tick = current_tick;
                dhcp_state = DHCP_REQUEST;
                dhcp_retry = 0;
                dhcp_last_tick = current_tick;
            }
            break;
        }
            
        default:
            return DHCP_FAIL;
    }
    
    return DHCP_RUNNING;
}

uint8_t DHCP_Get_State(void)
{
    return dhcp_state;
}

void DHCP_Set_State(uint8_t state)
{
    dhcp_state = state;
}

uint8_t DHCP_Get_IP_Info(DHCP_Info_t *info)
{
    if(info == NULL)
        return 0;
    
    my_memcpy((uint8_t*)info, (const uint8_t*)&dhcp_info, sizeof(DHCP_Info_t));
    return 1;
}

void DHCP_Stop(void)
{
    if(dhcp_socket != 0xFF)
    {
        close(dhcp_socket);
        dhcp_socket = 0xFF;
    }
    dhcp_state = DHCP_INIT;
}
