# CubeMX 配合说明

业务代码放在 **`App/`、`Hardware/`、`Protocol/`**，不要写在 CubeMX 会覆盖的区域外。

## CubeMX 会重生成的文件

- `Core/Src/*.c`、`Core/Inc/*.h`
- `Drivers/`、`Middlewares/`（若勾选）
- `MDK-ARM/startup_*.s`（部分）

## 当前工程已在 USER CODE 区保留的挂钩

| 文件 | USER CODE 位置 | 内容 |
|------|----------------|------|
| `Core/Src/freertos.c` | `RTOS_THREADS` | `App_TasksInit()` |
| `Core/Src/stm32h7xx_it.c` | （CubeMX 已生成） | `USART1_IRQHandler` / `USART3_IRQHandler` |
| `Hardware/uart/uart1_port.c` | — | `HAL_UART_RxCpltCallback`（USART1 协议 + USART3 PM2.5） |

**无需改 `usart.c`：** 协议与 PM2.5 回调在 `Hardware/uart/`。

## I2C / 传感器总线（内机硬件）

| 总线 | 引脚 | 设备 |
|------|------|------|
| I2C1 | PB6/PB7 | PCF85063 RTC |
| I2C4 | PH11/PH12 | BMP280、AHT20、ENS160 |
| USART3 RX | PB11 | PM2.5（9600，仅收） |
| USART3 TX | PC10 | 与 FSY-I 一致（PB10 让给 W25Q NCS） |

驱动参考 `probe/zjb/zjb`，环境传感器绑定 `hi2c4`。

## W25Q64 / QUADSPI（已在 `NeiJi.ioc` 预填，请用 CubeMX 打开核对）

与 `probe/FSY-I/RAD-I` 同板引脚：

| 信号 | 引脚 |
|------|------|
| IO0 | PF8 |
| IO1 | PF9 |
| IO2 | PE2 |
| IO3 | PF6 |
| CLK | PB2 |
| NCS | PB10 |

**Parameter Settings：**

- Clock Prescaler = **0**
- FIFO Threshold = **32**
- Sample Shifting = **Half Cycle**
- Flash Size = **22**（8MB，W25Q64）
- Chip Select High Time = **5 cycles**

### CubeMX 操作步骤

1. 打开 `NeiJi.ioc`，确认 **QUADSPI** 与上表引脚已分配。
2. 核对 **PB10** 为 `QUADSPI_BK1_NCS`，**USART3_TX** 在 **PC10**（勿与 PM2.5 / Flash 冲突）。
3. **Project Manager → Code Generator**：勾选 **Keep User Code**。
4. **Generate Code** → 生成 `Core/Src/quadspi.c`、`quadspi.h`，`main.c` 会插入 `MX_QUADSPI_Init()`。
5. Keil **Application/User/Core** 组加入 `quadspi.c`（`.uvprojx` 默认不自动更新）。
6. `Hardware/w25q/` 与 `app_tasks.c` 中的 `W25Q_Port_*` 自检已写好，生成后即可编译。

**不要手写 `Core/quadspi.c`**；外设只通过 `.ioc` 维护。

### 上电串口预期

```
[W25Q] JEDEC=0xEF4017 OK
[W25Q] selftest PASS @0x007FF000
```

## 你下次在 CubeMX 里改 USART1 后请确认

1. **NVIC**：USART1 global interrupt 仍勾选  
2. **Generate Code** 后打开 `freertos.c`，确认 `USER CODE RTOS_THREADS` 里仍有：
   ```c
   App_TasksInit();
   ```
3. 若 USER CODE 块被清空，把上面一行加回去即可  
4. **Keil 工程组**（`App/`、`Hardware/uart/`、`Protocol/`）CubeMX 不会动，无需重加  

## 新增源文件时

在 Keil 的 `Application/User/App`、`Hardware`、`Protocol` 组里 **Add Existing Files**，不要加到 `Core` 组。

## 协议文档（唯一来源）

仓库根目录：

- `辐射报警仪协议说明09.docx`
- `辐射报警仪协议命令和寄存器表10.xlsx`
- `辐射报警仪协议调试示例09.docx`
