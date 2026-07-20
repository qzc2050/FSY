# NeiJi 串口协议工具

通过 **USART1（115200）** 与内机通信，解析《辐射报警仪协议命令和寄存器表10》中的 Net Raw 协议。

## 安装

```bash
cd probe/Neiji/NeiJi/tools
pip install -r requirements.txt
```

## Windows 双击启动（工厂生产）

| 文件 | 说明 |
|------|------|
| **`启动上位机.bat`** | **推荐**：图形界面 — 实时参数 + **生产配置**（SN/型号/地址读写） |
| **`启动监听.bat`** | 命令行监听 0x23 |
| **`启动工具.bat`** | 菜单：图形界面 / 命令行 |

图形界面：串口下拉 + 刷新、连接/断开、**「生产配置」页**读写 SN/型号/协议地址、**「实时参数」页**自动解析 0x23，并含 **五分钟历史补拉**（写 reg108/112，收 0x23 start=0x0024）。

**「固件更新」页（串口 OTA）**：选择 App `.bin`（链接 `0x08020000`）→ 写 reg **200 → 208×N → 202**（每包等 `0x20` ACK）→ 设备写 Flag 并复位 → Boot 搬运。

也可带参数：`启动监听.bat COM5`

## 命令行用法

```bash
# 列出串口
python fsy_serial_tool.py --list

# 监听（推荐）：自动解析设备每秒 0x23 主动上传 + 打印 [net] 等日志
python fsy_serial_tool.py listen -p COM5

# 主动读寄存器 0x03
python fsy_serial_tool.py read -p COM5 --start 0x0001 --count 11

# 周期轮询
python fsy_serial_tool.py poll -p COM5 -i 2
```

## 实时寄存器（0x23，起始 0x0001）

| 地址 | 含义 | 换算 |
|------|------|------|
| 0x0001 | 剂量率 | raw ÷ 100 → μSv/h |
| 0x0003 | 温度 | raw ÷ 10 → ℃ |
| 0x0005 | 气压 | Pa |
| 0x0007 | 湿度 | % |
| 0x0009 | CO2 | ppm |
| 0x000B | PM2.5 | raw ÷ 10 → μg/m³ |
| 0x000D | 报警状态 | 位标志 |
| 0x000F | IO 状态 | 位标志 |

## 说明

- **0x23 主动上传**：11 个 **uint32** 小端，共 44 字节数据区；工具 `listen` 模式完整解析。
- **0x03 读保持寄存器**：NeiJi 当前固件应答为 **uint16** 块读，32 位传感器寄存器可能只返回低 16 位；要看完整数据请用 `listen`。
- CRC：**Modbus RTU CRC16**（与 `Protocol/fsy_crc.c` 一致）。
- 更完整 GUI 可参考 `probe/FSY-I/RAD-I/tools/net_raw_tester.py`。
