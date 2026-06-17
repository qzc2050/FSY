---
name: "radiation-protocol-parser"
description: "Parses and extracts radiation alarm instrument protocol documents from docs folder. Invoke when user needs to read protocol specs, register tables, or command definitions from .xlsx/.docx files."
---

# 辐射报警仪协议文档解析技能

## 技能概述

本技能专门用于解析项目 docs 文件夹下的辐射报警仪协议文档，包括：
- `辐射报警仪协议命令和寄存器表 07.xlsx` - Excel 格式的寄存器表和命令集
- `辐射报警仪协议说明 07.docx` - Word 格式的协议详细说明

## 文档位置

```
g:\Desktop\RAW_I_V20260324_mod_net_protocol _mod_self\docs\
├── 辐射报警仪协议命令和寄存器表 07.xlsx
└── 辐射报警仪协议说明 07.docx
```

## 提取的内容

### 1. 协议格式说明

#### 功能码 0x06/0x16/0x86 - 单寄存器读写
```
外机→内机 (写):  [地址][0x06][寄存器地址][寄存器值][CRC]
内机→外机 (应答): [地址][0x16][寄存器地址][寄存器值][CRC]
内机→外机 (错误): [地址][0x86][寄存器地址][错误码][CRC]
```

#### 功能码 0x10/0x20/0x90 - 多寄存器读写
```
外机→内机 (写):  [地址][0x10][起始地址][数量][字节数][数据...][CRC]
内机→外机 (应答): [地址][0x20][起始地址][数量][CRC]
内机→外机 (错误): [地址][0x90][起始地址][错误码][CRC]
```

#### 功能码 0x05/0x15/0x85 - 单寄存器读取
```
外机→内机 (读):  [地址][0x05][寄存器地址][数量 0x0001][CRC]
内机→外机 (应答): [地址][0x15][寄存器地址][寄存器值][CRC]
内机→外机 (错误): [地址][0x85][寄存器地址][错误码][CRC]
```

#### 功能码 0x03/0x13/0x83 - 多寄存器读取
```
外机→内机 (读):  [地址][0x03][起始地址][数量][CRC]
内机→外机 (应答): [地址][0x13][字节数][数据...][CRC]
内机→外机 (错误): [地址][0x83][寄存器地址][错误码][CRC]
```

#### 功能码 0x23/0x25 - 从机主动上传
```
内机→外机 (主动): [地址][0x23/0x25][字节数][数据...][CRC]
```

### 2. 地址定义

| 设备类型 | 地址范围 | 说明 |
|---------|---------|------|
| 主设备 (外机) | 0x20, 0x40, 0x60 | 固定地址 |
| 从设备 (内机) | 0x01-0x1F, 0x21-0x3F, 0x41-0x5F | 最多 32 个从机 |
| 转接板 | 0xEF | 内置传感器转接板 |

### 3. 寄存器映射表

#### 只读寄存器（传感器数据）

| 地址 | 数量 | 类型 | 变量名 | 功能 | 单位 | 缩放 |
|-----|------|-----|--------|------|------|------|
| 1 | 2 | uint32 | dose_rate | 辐射量 | uSv/h | *100 |
| 3 | 2 | int32 | temp | 温度 | ℃ | *10 |
| 5 | 2 | uint32 | press | 气压 | Pa | *1 |
| 7 | 2 | uint32 | hum | 湿度 | % | *1 |
| 9 | 2 | uint32 | co2 | CO2 含量 | ppm | *10 |
| 11 | 2 | uint32 | pm2d5 | PM2.5 | ug/m³ | *10 |
| 13 | 2 | uint32 | alarm_bit1 | 报警状态 | 32 位标志 | - |
| 15 | 2 | uint32 | status_bit | 设备状态 | 32 位标志 | - |
| 30 | 4 | uchar[8] | data_time | 时间戳 | 年月日时分秒 | - |
| 34 | 2 | uint32 | dose_rate | 辐射量 (5 分钟平均) | uSv/h/mSv/h/Sv/h | *100 |

#### 读写寄存器（配置参数）

| 地址 | 数量 | 类型 | 变量名 | 功能 | 单位 | 访问 |
|-----|------|-----|--------|------|------|------|
| 50 | 2 | uint32 | Alert_Threshold1 | 辐射量上阈值 | uSv/h | R/W |
| 52 | 2 | uint32 | Alert_Threshold2 | 辐射量下阈值 | uSv/h | R/W |
| 54 | 2 | uint32 | Alert_Threshold3 | 温度上阈值 | ℃ | R/W |
| 56 | 2 | uint32 | Alert_Threshold4 | 温度下阈值 | ℃ | R/W |
| 58 | 2 | uint32 | Alert_Threshold5 | 气压上阈值 | hPa | R/W |
| 60 | 2 | uint32 | Alert_Threshold6 | 气压下阈值 | hPa | R/W |
| 62 | 2 | uint32 | Alert_Threshold7 | 湿度上阈值 | % | R/W |
| 64 | 2 | uint32 | Alert_Threshold8 | 湿度下阈值 | % | R/W |
| 66 | 2 | uint32 | Alert_Threshold9 | CO2 上阈值 | ppm | R/W |
| 68 | 2 | uint32 | Alert_Threshold10 | CO2 下阈值 | ppm | R/W |
| 70 | 2 | uint32 | Alert_Threshold11 | PM2.5 上阈值 | ug/m³ | R/W |
| 72 | 2 | uint32 | Alert_Threshold12 | PM2.5 下阈值 | ug/m³ | R/W |
| 82 | 2 | uint32 | alarm_biten | 报警使能 | 32 位标志 | R/W |
| 86 | 8 | char[16] | serialnum | 序列号 | 16 字节 ASCII | R/W |
| 94 | 4 | uchar[8] | data_time | 系统时间 | 年月日时分秒 | R/W |
| 108 | 4 | uchar[8] | data_time_start | 历史数据开始时间 | - | R/W |
| 112 | 4 | uchar[8] | data_time_end | 历史数据结束时间 | - | R/W |
| 120 | 1 | uint16 | reboot | 设备重启控制 | 0x0001=重启 | W |
| 121 | 1 | uint16 | address | 通信地址 | 0x01-0x7F | R/W |
| 122 | 1 | uint16 | control_bit | 声光屏控制 | bit0-2 | R/W |

#### 固件升级寄存器

| 地址 | 数量 | 类型 | 变量名 | 功能 |
|-----|------|-----|--------|------|
| 200 | 2 | uint32 | file_total | 文件总长度 |
| 202 | 2 | uint32 | file_index | 当前文件索引 |
| 204 | 16 | char[32] | file_data | 文件数据块 |

### 4. 报警状态位定义（寄存器地址 13）

```
bit0:  辐射上阈值报警
bit1:  辐射下阈值报警
bit2:  辐射检测离线
bit3:  保留
bit4:  温度上阈值报警
bit5:  温度下阈值报警
bit6:  温度检测离线
bit7:  保留
bit8:  气压上阈值报警
bit9:  气压下阈值报警
bit10: 气压检测离线
bit11: 保留
bit12: 湿度上阈值报警
bit13: 湿度下阈值报警
bit14: 湿度检测离线
bit15: 保留
bit16: CO2 上阈值报警
bit17: CO2 下阈值报警
bit18: CO2 检测离线
bit19: 保留
bit20: PM2.5 上阈值报警
bit21: PM2.5 下阈值报警
bit22: PM2.5 检测离线
bit23: 保留
bit24: 声报警损坏
bit25: 保留
bit26: 声报警离线
bit27: 保留
bit28: 光报警损坏
bit29: 保留
bit30: 光报警离线
bit31: 保留
```

### 5. 设备状态位定义（寄存器地址 15）

```
bit0:  门状态 (0=打开，1=关闭)
bit1:  PM2.5 电源状态 (0=低，1=高)
bit2:  PM2.5 复位引脚 (0=复位，1=正常)
bit3:  蓝牙音箱暂停键 (BT_PIO2)
bit4:  蓝牙音箱音量 - 键 (BT_PIO3)
bit5:  蓝牙音箱音量 + 键 (BT_PIO4)
bit6:  音频静音 (AUDIO_MUTE, 1=静音)
bit7:  风扇开关 (0=关，1=开)
bit8:  USB 选择 (0=USB3 默认，1=USB4 下载)
bit9:  LoRa 电源使能
bit10: LoRa 模式 M1
bit11: LoRa 模式 M0
```

### 6. 网络通信

#### UDP 组播发现
```
组播地址：236.2.3.6:2468
广播格式：产品型号，产品序列号，IP 地址，控制端口，数据流端口，协议地址，协议类型
示例：FSY-I,1905CCM0101,192.168.2.101,5001,5000,1,0
```

#### TCP Socket 端口
- Socket 0: 5000 (默认数据端口)
- Socket 1: 5001 (控制端口)
- Socket 2: 5002 (备用端口)

### 7. 错误码定义

| 错误码 | 名称 | 说明 |
|-------|------|------|
| 0x0002 | Illegal Data Address | 请求地址超出内存范围或访问未实现地址 |
| 0x0003 | Illegal Data Value | 请求的值不合法 |
| 0x0004 | Slave Device Failure | 从站设备内部错误 |

## 使用场景

1. **查询寄存器地址**: 快速查找特定功能的寄存器地址和数据格式
2. **理解协议帧结构**: 解析 Modbus 协议帧格式和 CRC 校验
3. **实现通信功能**: 基于协议文档实现网口/串口通信
4. **调试问题**: 根据错误码和状态位诊断设备问题
5. **配置参数**: 了解报警阈值、设备地址等配置方法

## 相关代码文件

协议实现相关的代码文件：
- `dev_protocol/net_raw/` - Net_Raw 协议实现
- `HARDWARE/W5500/` - W5500 网络驱动
- `FreeRTOS/APP/freertos_app.c` - 任务调度和网络任务

## 注意事项

1. **字节序**: 所有多字节数据使用小端序（Little-Endian）
2. **CRC 校验**: 使用 Modbus CRC-16 校验
3. **地址范围**: 从机地址 0x01-0x1F，避免使用 0x00（广播）和 0x7F（保留）
4. **时间格式**: data_time[8] 数组，索引 0-5 分别为年月日时分秒，6-7 预留
5. **单位换算**: 注意各传感器的缩放因子（如辐射量*100，温度*10）
