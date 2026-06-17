# CAN / NETWORK 统一接口说明

## 概述

net_raw 协议栈通过统一的注册接口同时支持 CAN 和 NETWORK（TCP/UDP）设备。
协议层**不区分设备类型**，行为完全由注册时的配置参数决定：

| 参数 | 含义 | CAN 典型值 | NETWORK 典型值 |
|------|------|-----------|---------------|
| `txb_size` | 单次 transmit() 最大字节数 | 8 | 1024 |
| `rxb_size` | 单次 receive() 最大字节数 | 8 | 1024 |
| `reasm_sz` | 重组缓冲大小 | 128 | 1024 |

**关键特性**：
- 发送时自动按 `txb_size` 分片（CAN 自动 8 字节分包，NETWORK 一次发完）
- 接收时统一通过重组缓冲累积 + CRC 滑窗识别完整帧
- 支持 TCP 粘包（多帧一包到达）和分片（一帧拆成多包）

## 多设备共存示例

```c
#include "./dev_protocol/net_raw/net_raw_protocol.h"

static Net_Device_t *g_network_dev = NULL;
static Net_Device_t *g_can_dev = NULL;

void Multi_Device_Init(void)
{
    Net_Periph_t *can_ph, *net_ph;
    Net_Device_Base_Config_t dcfg;
    
    /* ========== 注册 CAN 设备 ========== */
    can_ph = Net_Periph_Register("CAN0", CAN_Periph_Init, NULL, CAN_Transmit, CAN_Receive);
    
    dcfg.qcfg.txq_depth = 8;
    dcfg.qcfg.rxq_depth = 8;
    dcfg.qcfg.txb_size = 8;          /* CAN 单帧 8 字节 */
    dcfg.qcfg.rxb_size = 8;
    dcfg.period = 10;
    dcfg.addr = 0x01;
    dcfg.reg_tb = NULL;
    dcfg.reg_sz = 256;
    dcfg.reasm_sz = 128;             /* 重组缓冲需大于最长 Modbus 帧 */
    
    g_can_dev = Net_Device_Register(can_ph, "CAN_Slave", CAN_Begin, CAN_End, &dcfg);
    
    /* ========== 注册 NETWORK 设备 ========== */
    net_ph = Net_Periph_Register("ETH0", W5500_Init, NULL, W5500_Transmit, W5500_Receive);
    
    dcfg.qcfg.txq_depth = 16;
    dcfg.qcfg.rxq_depth = 16;
    dcfg.qcfg.txb_size = 1024;       /* 网络设备可一次发送整帧 */
    dcfg.qcfg.rxb_size = 1024;
    dcfg.period = 5;
    dcfg.addr = 0x01;
    dcfg.reg_tb = NULL;
    dcfg.reg_sz = 256;
    dcfg.reasm_sz = 1024;
    
    g_network_dev = Net_Device_Register(net_ph, "W5500_Slave", W5500_Begin, W5500_End, &dcfg);
}
```

## 关键配置说明

### txb_size —— 发送分片控制

发送宏 `Net_Transmit_Data` 自动按 `txb_size` 分片循环发送：
- CAN（txb_size=8）：22 字节数据 → 自动分成 8+8+6 三次发送
- NETWORK（txb_size=1024）：22 字节数据 → 一次发送完毕

### rxb_size —— 接收 / CRC 搜索上限

`rxb_size` 同时决定 CRC 滑窗搜索的最大帧长上限。

### reasm_sz —— 重组缓冲大小

- CAN 设备：需大于最长预期 Modbus 帧（如 128 字节）
- NETWORK 设备：一般与 rxb_size 相同即可

## 调试信息示例

```
/* CAN 分包发送 */
TX(chunk) -> 01 03 00 00 00 02 C4 0B  (第 1 片 8 字节)
TX(chunk) -> 12 34 56 78 9A BC        (第 2 片 6 字节)

/* 重组接收 */
RX(reasm) -> 01 03 04 12 34 56 78 CRC (完整帧)
```

## 兼容性

- 可以只注册 NETWORK 设备（传统用法）
- 可以只注册 CAN 设备
- 可以同时注册多个不同配置的设备
