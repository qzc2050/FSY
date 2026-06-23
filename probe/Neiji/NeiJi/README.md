# NeiJi 内机固件（重写版）

FSY-I 辐射探头内机程序，在 `probe/Neiji/NeiJi` **从零重写**。  
旧同事代码位于 `probe/FSY-I/RAD-I/`，**仅作参考**，不直接拷贝。

## 打开工程

1. Keil µVision 5 → 打开 `MDK-ARM/NeiJi.uvprojx`
2. CubeMX → 打开 `NeiJi.ioc`（外设配置在此维护）
3. 芯片：**STM32H743IIT6**（LQFP176）

## 当前状态（2026-06-18）

| 项 | 状态 |
|----|------|
| CubeMX + Keil 空工程 | ✅ 可编译出 `NeiJi.hex` |
| FreeRTOS CMSIS-RTOS V2 | ✅ defaultTask + appMain |
| USART1 调试日志 | ✅ PB14/PB15，115200，`[NeiJi] heartbeat` |
| 协议 / 盖革 / 以太网 / UI | 🔴 待开发 |

## 目录约定

```
NeiJi/
├── Core/              CubeMX 生成（勿手改，用 .ioc 重新生成）
├── App/               应用入口、任务编排
├── Hardware/          板级驱动（log、盖革、传感器、Flash…）
├── Protocol/          辐射报警仪协议（与 Android 对齐）
├── Middlewares/       FreeRTOS
├── Drivers/           HAL / CMSIS
├── MDK-ARM/           Keil 工程
├── docs/              本工程文档
└── NeiJi.ioc          CubeMX 配置
```

**规则：** `Core/` 只放 CubeMX 代码；业务逻辑放 `App/`、`Hardware/`、`Protocol/`。

## 与旧工程 / Android 的关系

```
Android NetShield  ←TCP/UDP→  NeiJi（本工程，型号 FSY-I）
                              ↕ CAN/串口
                         zjb 转接板（0xEF，环境传感器）
```

- 协议寄存器表：`probe/zjb/zjb/辐射报警仪协议命令和寄存器表0502.csv`
- 旧实现参考：`probe/FSY-I/RAD-I/dev_protocol/net_raw/`
- Android 端：`app/src/main/java/com/raydose/netshield/net/`

## 外设迁移计划（对照 FSY-I，在 CubeMX 逐步添加）

| 模块 | 旧工程 | 说明 |
|------|--------|------|
| W5500 以太网 | SPI | TCP 通信主通道 |
| 盖革计数 | TIM + DMA | 剂量率 |
| QSPI Flash | QUADSPI | 5 分钟历史 |
| 环境传感器 | I2C | BME280/AHT20/ENS160/PM2.5 |
| LVGL 屏 | LTDC/SPI | 本地 UI |
| LoRa / CAN | 按需 | 备用链路 |
| USB MSC | USB OTG | 可选 |

## 开发顺序建议

1. **串口日志 + 任务框架**（已完成）
2. **协议帧编解码** + USART 联调
3. **W5500 + TCP** → Android 组播发现 + 0x23 实时
4. **盖革 + 剂量率算法**
5. **QSPI 5 分钟历史** + 0x006C 回补
6. 环境传感器、LVGL、OTA

详见 `docs/开发计划.md` 与仓库根目录 `docs/内机固件开发进度.md`。
