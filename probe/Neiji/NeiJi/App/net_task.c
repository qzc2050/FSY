#include "net_task.h"

#include "cmsis_os.h"

#include "device.h"
#include "w5500.h"
#include "w5500_dhcp.h"
#include "net_tcp.h"
#include "fsy_link.h"
#include "uart_ringbuf.h"

#include <stdio.h>

#ifndef NET_TASK_PHASE
#define NET_TASK_PHASE  4
#endif

static void NetTask(void *argument);

static osThreadId_t netTaskHandle;
static const osThreadAttr_t netTaskAttributes = {
    .name = "netTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

static uint32_t s_recover_log_tick = 0U;
static UartRingBuf s_tcp_rx;
static uint8_t s_tcp_rx_storage[NET_TCP_RX_CAP];

static int NetTcpWriteAdapter(const uint8_t *data, uint16_t len)
{
    return Fsy_Link_WriteTcp(data, len);
}

static void NetTask_PrintDiag(void)
{
    uint8_t ip[4];
    uint8_t mac[6];
    uint8_t phy_status;

    printf("\r\n========== W5500 Network ==========\r\n");
    printf("Chip ver: 0x%02X\r\n", IINCHIP_READ(VERSIONR));
    getSHAR(mac);
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    getSIPR(ip);
    printf("IP: %u.%u.%u.%u\r\n",
           (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
    phy_status = getPHYCFGR();
    printf("PHY: link=%s speed=%s duplex=%s\r\n",
           (phy_status & LINK) ? "UP" : "DOWN",
           (phy_status & SPD) ? "100M" : "10M",
           (phy_status & DPX) ? "Full" : "Half");
    printf("===================================\r\n\r\n");
}

void Net_TaskInit(void)
{
    netTaskHandle = osThreadNew(NetTask, NULL, &netTaskAttributes);
    if (netTaskHandle == NULL) {
        printf("[net] osThreadNew failed\r\n");
    }
}

static void NetTask(void *argument)
{
    (void)argument;

    (void)osDelay(500);
    printf("[net] task start (phase %d)\r\n", NET_TASK_PHASE);

#if (NET_TASK_PHASE >= 2)
    printf("[net] W5500_Hw_Prepare...\r\n");
    W5500_Hw_Prepare();
    printf("[net] Reset_W5500...\r\n");
    Reset_W5500();
    printf("[net] set_w5500_default...\r\n");
    set_w5500_default();
    printf("[net] set_w5500_network...\r\n");
    set_w5500_network();
    printf("[net] phase %d init done\r\n", NET_TASK_PHASE);
#endif

#if (NET_TASK_PHASE >= 3)
    NetTask_PrintDiag();
#endif

#if (NET_TASK_PHASE >= 4)
    UartRingBuf_Init(&s_tcp_rx, s_tcp_rx_storage, (uint16_t)sizeof(s_tcp_rx_storage));
    printf("[net] FSY TCP listen (after DHCP)\r\n");
#endif

#if (NET_TASK_PHASE < 4)
    for (;;) {
        printf("[net] phase %d idle\r\n", NET_TASK_PHASE);
        (void)osDelay(2000);
    }
#else
    for (;;) {
        W5500_PhyLink_DebouncedLoopBegin();
        DHCP_Client_Task();
        DHCP_Watchdog_Task();

        if (W5500_Is_Network_Recovering()) {
            uint32_t now = HAL_GetTick();

            if ((s_recover_log_tick == 0U) || ((now - s_recover_log_tick) >= 1000U)) {
                s_recover_log_tick = now;
                printf("[W5500] network recovering...\r\n");
            }

            (void)osDelay(2);
            continue;
        }

        s_recover_log_tick = 0U;

        UDP_Broadcast_Task();
        Net_Tcp_PollRx(&s_tcp_rx);
        Fsy_Link_ProcessRx(&s_tcp_rx, NetTcpWriteAdapter);

        (void)osDelay(2);
    }
#endif
}
