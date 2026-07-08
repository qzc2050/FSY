#include "device.h"
#include "config.h"
#include "w5500.h"
#include "socket.h"
#include "network_cmd.h"
#include "w5500_dhcp.h"
#include "net_tcp.h"
#include "net_config.h"
#include "spi.h"
#include "device_config.h"

#include "cmsis_os.h"

#include <stdio.h>
#include <string.h>

CONFIG_MSG ConfigMsg;

uint8 txsize[MAX_SOCK_NUM] = {8, 2, 1, 1, 1, 1, 1, 1};
uint8 rxsize[MAX_SOCK_NUM] = {2, 8, 1, 1, 1, 1, 1, 1};

#define W5500_TCP_RTR_UNIT100US  (20000U)
#define W5500_TCP_RCR            (5U)

static void delay_ms(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning) {
        (void)osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
}

void W5500_Hw_Prepare(void)
{
    W5500_SpiMutexInit();
    W5500_SpiRecover();
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
#if W5500_RST_PIN_CONNECTED
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
#endif
}

static void W5500_WaitChipReset(void)
{
    uint32_t t0 = HAL_GetTick();

    while ((IINCHIP_READ(MR) & MR_RST) != 0U) {
        if ((HAL_GetTick() - t0) > 500U) {
            printf("[W5500] chip reset wait timeout\r\n");
            break;
        }
        delay_ms(1);
    }
}

static void W5500_CloseAllSockets(void)
{
    uint8_t i;

    for (i = 0U; i < MAX_SOCK_NUM; i++) {
        close(i);
    }
}

#if !W5500_RST_PIN_CONNECTED
/** 冷上电：等 PHY link 连续稳定后再做脉冲复位，避免与交换机同时爬升 */
static void W5500_WaitPhyLinkStable(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint32_t up_since = 0U;

    while ((HAL_GetTick() - t0) < timeout_ms) {
        if ((getPHYCFGR() & LINK) != 0U) {
            if (up_since == 0U) {
                up_since = HAL_GetTick();
            } else if ((HAL_GetTick() - up_since) >= 300U) {
                printf("[W5500] PHY link stable (%ums)\r\n",
                       (unsigned)(HAL_GetTick() - up_since));
                return;
            }
        } else {
            up_since = 0U;
        }
        delay_ms(10);
    }

    printf("[W5500] PHY link wait timeout (%ums)\r\n", (unsigned)timeout_ms);
}

static void W5500_PhyPulseReset(void)
{
    uint8_t phy;
    uint32_t t0;

    /*
     * MCU 软复位时 W5500 仍供电、PHY 链路不断，DHCP/组播易卡在旧链路状态。
     * 脉冲复位 PHY 模拟断电上电时的 link down/up。
     */
    printf("[W5500] PHY pulse reset (RST pin NC)...\r\n");
    IINCHIP_WRITE(PHYCFGR, 0x00U);
    delay_ms(20);
    IINCHIP_WRITE(PHYCFGR, (uint8_t)(RST | OPMD | OPMDC_100M_10M));

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < 3000U) {
        phy = getPHYCFGR();
        if ((phy & LINK) != 0U) {
            break;
        }
        delay_ms(10);
    }

    delay_ms(200);

    phy = getPHYCFGR();
    printf("[W5500] PHY after pulse: 0x%02X link=%s\r\n",
           phy, (phy & LINK) ? "UP" : "DOWN");
}
#endif

static void W5500_DoReset(void)
{
#if W5500_RST_PIN_CONNECTED
    printf("[W5500] hardware reset (GPIO)...\r\n");
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    delay_ms(20);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    delay_ms(200);
#else
    printf("[W5500] soft reset (MR_RST, RST pin NC)...\r\n");
    iinchip_init();
    W5500_WaitChipReset();
    delay_ms(10);
#endif
}

static void W5500_PrintSpiStats(void)
{
    uint32_t ok;
    uint32_t err;
    uint32_t st;
    uint32_t ecode;

    W5500_SpiGetStats(&ok, &err, &st, &ecode);
    printf("[W5500] SPI ok=%lu err=%lu last_st=%lu errcode=0x%08lX hstate=%u\r\n",
           (unsigned long)ok, (unsigned long)err, (unsigned long)st,
           (unsigned long)ecode, (unsigned)hspi1.State);
}

void Reset_W5500(void)
{
    uint8_t pdata = 0;
    uint8_t i;

    W5500_Hw_Prepare();
    W5500_DoReset();

    printf("[W5500] read version...\r\n");
    pdata = W5500_SpiRawReadVersion();
    if (pdata == 0U) {
        pdata = IINCHIP_READ(VERSIONR);
    }
    printf("w5500 ver = 0x%02x\r\n", pdata);
    W5500_PrintSpiStats();

    if (pdata != 0x04) {
        printf("[W5500] version 0x%02X != 0x04, retry (max 3)...\r\n", pdata);
        for (i = 0; i < 3U; i++) {
            W5500_SpiRecover();
            W5500_DoReset();

            pdata = IINCHIP_READ(VERSIONR);
            printf("[W5500] retry %u ver=0x%02X\r\n", (unsigned)(i + 1U), pdata);
            if (pdata == 0x04) {
                break;
            }
        }
        W5500_PrintSpiStats();
        if (pdata != 0x04) {
            printf("[W5500] version read failed, check SPI CS/MISO/SCK/MOSI\r\n");
        }
    }
}

static void mac_force_unicast(uint8_t *m)
{
    if (m != NULL) {
        m[0] &= (uint8_t)~0x01u;
    }
}

void set_w5500_network(void)
{
    mac_force_unicast(ConfigMsg.mac);

    setSHAR(ConfigMsg.mac);
    setSUBR(ConfigMsg.sub);
    setGAR(ConfigMsg.gw);

    sysinit(txsize, rxsize);
    W5500_CloseAllSockets();
    setRTR(W5500_TCP_RTR_UNIT100US);
    setRCR(W5500_TCP_RCR);

#if !W5500_RST_PIN_CONNECTED
    W5500_WaitPhyLinkStable(5000U);
    if (ConfigMsg.dhcp) {
        /* 软复位时 W5500 仍供电，需脉冲 PHY；静态/冷上电 W5500 同步复位，不再脉冲 */
        W5500_PhyPulseReset();
    }
    W5500_PhyLink_ResetDebouncer();
#endif

    if (ConfigMsg.dhcp) {
        {
            uint8_t zero_ip[4] = {0, 0, 0, 0};
            setSIPR(zero_ip);
        }
        W5500_DHCP_Init();
    } else {
        setSIPR(ConfigMsg.lip);
        UDP_Broadcast_Init();
        if (W5500_Is_IP_Valid_Buf(ConfigMsg.lip)) {
            W5500_SyncBoundIp(ConfigMsg.lip);
            Net_Tcp_RebindOnIpChange();
        }
    }
}

void set_w5500_default(void)
{
    uint8 ip[4] = DEFAULT_IP_ADDR;
    uint8 mac[6] = {0xCC, 0x08, 0xDC, 0x01, 0x01, 0x01};
    uint8 sub[4] = DEFAULT_SUBNET_MASK;
    uint8 dns[4] = {8, 8, 8, 8};
    uint8 gw[4] = DEFAULT_GATEWAY;
    uint8 i;

    for (i = 0; i < 4; i++) {
        ConfigMsg.lip[i] = ip[i];
        ConfigMsg.sub[i] = sub[i];
        ConfigMsg.gw[i] = gw[i];
        ConfigMsg.dns[i] = dns[i];
    }
    for (i = 0; i < 6; i++) {
        ConfigMsg.mac[i] = mac[i];
    }
    mac_force_unicast(ConfigMsg.mac);

    ConfigMsg.dhcp = USE_DHCP;
    ConfigMsg.debug = 1;
    ConfigMsg.fw_len = 0;
    ConfigMsg.state = NORMAL_STATE;
    ConfigMsg.sw_ver[0] = FW_VER_HIGH;
    ConfigMsg.sw_ver[1] = FW_VER_LOW;

    if (DeviceConfig_IsReady() != 0U) {
        uint8_t dhcp_en = USE_DHCP;
        uint8_t static_ip[4];

        DeviceConfig_GetNetwork(&dhcp_en, static_ip);
        ConfigMsg.dhcp = dhcp_en;
        for (i = 0; i < 4; i++) {
            ConfigMsg.lip[i] = static_ip[i];
        }
    }

    printf("[W5500] network mode: %s\r\n", ConfigMsg.dhcp ? "DHCP" : "STATIC");
    if (!ConfigMsg.dhcp) {
        printf("[W5500] static IP=%u.%u.%u.%u\r\n",
               (unsigned)ConfigMsg.lip[0], (unsigned)ConfigMsg.lip[1],
               (unsigned)ConfigMsg.lip[2], (unsigned)ConfigMsg.lip[3]);
    }
}
