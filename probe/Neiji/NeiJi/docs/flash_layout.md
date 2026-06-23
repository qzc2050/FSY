# NeiJi 片内 Flash 分区说明

> MCU：STM32H743IITx，片内 Flash **2MB**（双 Bank，各 8 扇区 × **128KB**）  
> 与 FSY-I `ota_tt/RAD-I-IAP/HARDWARE/ota_bl.h` 对齐，便于后续共用 Bootloader。

## 四区总览

| 区域 | 宏前缀 | 起始地址 | 大小 | 扇区 | 现阶段 |
|------|--------|----------|------|------|--------|
| **Boot** | `BOOT_*` | `0x0800_0000` | 128KB | 0 | 空置，预留给 IAP |
| **App** | `APP_*` | `0x0802_0000` | 896KB | 1–7 | NeiJi 应用程序 |
| **App Download** | `APP_DOWNLOAD_*` | `0x0810_0000` | 896KB | 8–14 | 空置，OTA 接收固件 |
| **Set** | `SET_*` | `0x081E_0000` | 128KB | 15 | 工厂配置 + OTA 标志 |

代码中统一引用：`Hardware/storage/flash_layout.h`。

```
0x0800_0000  ┌──────────────┐
             │    Boot      │  128KB   将来 IAP，负责校验/搬运 Download→App
0x0802_0000  ├──────────────┤
             │              │
             │     App      │  896KB   当前业务固件（NeiJi）
             │              │
0x0810_0000  ├──────────────┤
             │              │
             │ App Download │  896KB   OTA 时上位机写入，Boot 校验后拷贝到 App
             │              │
0x081E_0000  ├──────────────┤
             │     Set      │  128KB   序列号/地址/型号等 + OTA 升级标志
0x0820_0000  └──────────────┘
```

## Set 子布局（扇区 15 内部）

H7 擦除粒度是 **128KB 整扇区**，改 SN 与改 OTA 标志都必须 **整扇区读-改-擦-写**。

| 偏移 | 地址 | 用途 | 大小（建议） |
|------|------|------|--------------|
| `0x0000` | `0x081E_0000` | OTA Flag（`OtaFlag_t`，后续 IAP 用） | 256B |
| `0x0400` | `0x081E_0400` | **DeviceCfg**（序列号、型号、协议地址…） | ≤512B |
| `0x0800` | `0x081E_0800` | 配置备份槽（可选 A/B） | ≤512B |
| 其余 | — | `0xFF` 保留 | — |

工厂配置 **只动 Set 区**，与 App / Download **物理隔离** → OTA 擦 Download、换 App **不会清 SN**。

## 现阶段（不做 OTA）怎么做

1. **App 链接范围**  
   - **推荐（面向 IAP）**：IROM 起始 `0x0802_0000`，长度 `0x000E_0000`（896KB）。  
   - **当前 Keil 工程**仍为 `0x0800_0000`–`0x081F_FFFF` 全 2MB；在实现配置持久化前，应改为上述范围，**避免 hex 覆盖 Download/Set**。

2. **Boot 区**  
   - 暂空（全 `0xFF` 或调试器直接跑 App）。  
   - 上 IAP 后：Boot 占扇区 0，App 仍从 `0x0802_0000` 起，**无需改 App 链接地址**。

3. **App Download 区**  
   - 代码中 **不要** 写常量、不要链接；仅头文件预留。

4. **Set 区**  
   - 下一步实现 `DeviceConfig_Init()`：读 `SET_DEVICE_CFG_ADDR`，无效则写出厂默认。  
   - 协议 0x0056 / 0x0079 等读写最终 **Commit 到 Set**。

## 后续 OTA 流程（规划，暂不实现）

1. App 收包 → 写入 **App Download**（`0x0810_0000` 起）。  
2. 校验 CRC → 在 **Set** 写 `OtaFlag_t`（`status=PENDING`）。  
3. 复位 → **Boot** 把 Download 拷贝到 **App**，更新 Flag，跳 App。  
4. **Set 中 DeviceCfg 不动**。

## 与外部 W25Q 的关系

FSY-I 还把 5 分钟历史、大块配置放在 **外部 W25Q 末尾**。NeiJi 首版工厂项（SN/型号/地址）放 **片内 Set** 即可；以后历史记录再上 W25Q，与本文四区无关。

## 相关文件（规划）

| 文件 | 状态 |
|------|------|
| `Hardware/storage/flash_layout.h` | 已定义四区 |
| `Hardware/storage/device_config.c` | 待做：读写 Set / 上电加载 |
| `MDK-ARM` scatter / IROM | 待做：限制 App ≤ 896KB |
| Boot 工程（可复用 FSY-I IAP） | 待做 |
