#include "device.h"
#include "config.h"
#include "w5500.h"
#include "string.h"
#include "network_cmd.h"
#include "w5500_dhcp.h"

#include <stdio.h>

/* 使用 network_cmd.h 中的 DHCP 宏定义 */
#ifndef USE_DHCP
#define USE_DHCP    (1)
#endif

/* W5500 TCP 重传：RTR 单位 100µs；OTA Flash 写期间需更长容忍再报 TIMEOUT */
#define W5500_TCP_RTR_UNIT100US  (20000U)  /* 2s/次 */
#define W5500_TCP_RCR            (5U)

CONFIG_MSG  ConfigMsg, RecvMsg;

uint8 txsize[MAX_SOCK_NUM] = {8,2,1,1,1,1,1,1};
uint8 rxsize[MAX_SOCK_NUM] = {2,8,1,1,1,1,1,1};

void w5500_rst_io_configuration(void)
{
	GPIO_InitTypeDef GPIO_Initure;

	__HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_Initure.Pin = W5500_LINK_Pin;
  GPIO_Initure.Mode = GPIO_MODE_INPUT;
  GPIO_Initure.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(W5500_LINK_GPIO_Port, &GPIO_Initure);

  GPIO_Initure.Pin = W5500_RST_PIN;
	GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_Initure.Pull = GPIO_NOPULL;
	GPIO_Initure.Alternate = 0;
	GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(W5500_RST_GPIO_PORT, &GPIO_Initure);

  GPIO_Initure.Pin = W5500_CS_Pin;
	GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_Initure.Pull = GPIO_NOPULL;
	GPIO_Initure.Alternate = 0;
	GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(W5500_CS_GPIO_Port, &GPIO_Initure);

	HAL_GPIO_WritePin(W5500_RST_GPIO_PORT, W5500_RST_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

void Reset_W5500(void)
{
  unsigned char pdata = 0;
  uint8_t i = 0;

  HAL_GPIO_WritePin(W5500_RST_GPIO_PORT, W5500_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(W5500_RST_GPIO_PORT, W5500_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(200);

  pdata = IINCHIP_READ(VERSIONR);
  printf("w5500 ver = 0x%02x\r\n", pdata);

  if (pdata != 0x04)
  {
    for (i = 0; i < 20; i++)
    {
      HAL_GPIO_WritePin(W5500_RST_GPIO_PORT, W5500_RST_PIN, GPIO_PIN_RESET);
      HAL_Delay(20);
      HAL_GPIO_WritePin(W5500_RST_GPIO_PORT, W5500_RST_PIN, GPIO_PIN_SET);
      HAL_Delay(200);

      pdata = IINCHIP_READ(VERSIONR);
      if (pdata == 0x04)
      {
        break;
      }
    }
  }
}

/* IEEE 802.3：以太网源地址首字节 I/G 位须为 0（单播）。若 LSB=1 会被判为组播，交换机/路由器常丢弃。 */
static void mac_force_unicast(uint8_t *m)
{
  if (m != NULL)
  {
    m[0] &= (uint8_t)~0x01u;
  }
}

void set_w5500_network(void)
{
//  uint8 ip[4];

  mac_force_unicast(ConfigMsg.mac);

  /* 检查是否启用 DHCP */
  if(ConfigMsg.dhcp)
  {
    /* DHCP 模式：使用动态 IP */
    // printf("[网络] 启用 DHCP 动态 IP 分配...\r\n");
    
    /* 先设置默认 MAC 和初始配置 */
    setSHAR(ConfigMsg.mac);
    setSUBR(ConfigMsg.sub);
    setGAR(ConfigMsg.gw);
    setSIPR(ConfigMsg.lip);  // 初始为默认 IP（DHCP 获取前使用）
    
    sysinit(txsize, rxsize);
    setRTR(W5500_TCP_RTR_UNIT100US);
    setRCR(W5500_TCP_RCR);
    
    /* 初始化 DHCP */
    W5500_DHCP_Init();
    
    /* UDP 组播在 DHCP 拿到有效 IP 后由 UDP_Broadcast_Task 重新绑定 */
  }
  else
  {
    /* 静态 IP 模式 */
    printf("[网络] 使用静态 IP 配置...\r\n");
    
    setSHAR(ConfigMsg.mac);
    setSUBR(ConfigMsg.sub);
    setGAR(ConfigMsg.gw);
    setSIPR(ConfigMsg.lip);

    sysinit(txsize, rxsize);
    setRTR(W5500_TCP_RTR_UNIT100US);
    setRCR(W5500_TCP_RCR);

    UDP_Broadcast_Init();
  }

  /* 打印网络配置 */
  // getSIPR(ip);
  // printf("IP  : %d.%d.%d.%d\r\n", ip[0],ip[1],ip[2],ip[3]);
  // getSUBR(ip);
  // printf("SUB : %d.%d.%d.%d\r\n", ip[0],ip[1],ip[2],ip[3]);
  // getGAR(ip);
  // printf("GW  : %d.%d.%d.%d\r\n", ip[0],ip[1],ip[2],ip[3]);
}

void set_w5500_default(void)
{
  /* 使用宏定义的默认 IP 地址（DHCP 获取前使用） */
  uint8 ip[4] = DEFAULT_IP_ADDR;
  /* 首字节须为偶数（单播）；勿用 0xCD 等 I/G=1 的"组播样式"地址作源 MAC */
  uint8 mac[6]={0xcc,0x08,0xdc,0x11,0x11,0x11};
  uint8 sub[4] = DEFAULT_SUBNET_MASK;
  uint8 dns[4]={8, 8, 8, 8};
  uint8 gw[4] = DEFAULT_GATEWAY;

  /* 按字节拷贝 - 避免对齐问题 */
  for(uint8 i = 0; i < 4; i++) ConfigMsg.lip[i] = ip[i];
  for(uint8 i = 0; i < 4; i++) ConfigMsg.sub[i] = sub[i];
  for(uint8 i = 0; i < 6; i++) ConfigMsg.mac[i] = mac[i];
  mac_force_unicast(ConfigMsg.mac);
  for(uint8 i = 0; i < 4; i++) ConfigMsg.dns[i] = dns[i];
  
  for(uint8 i = 0; i < 4; i++) ConfigMsg.gw[i] = gw[i];

  /* 使用宏定义配置 DHCP 模式 */
  ConfigMsg.dhcp = USE_DHCP;  // 1=动态 IP, 0=静态 IP
  ConfigMsg.debug = 1;
  ConfigMsg.fw_len = 0;
  ConfigMsg.state = NORMAL_STATE;
  ConfigMsg.sw_ver[0] = FW_VER_HIGH;
  ConfigMsg.sw_ver[1] = FW_VER_LOW;
  
  printf("[W5500] 默认网络模式：%s\r\n", USE_DHCP ? "DHCP" : "STATIC");
  printf("[W5500] 默认 IP: %d.%d.%d.%d (DHCP 获取前使用)\r\n", ip[0], ip[1], ip[2], ip[3]);
}
