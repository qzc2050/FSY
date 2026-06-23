# 协议层（待实现）

与 Android 主机、转接板共用 **辐射报警仪协议**（Modbus RTU 风格）。

## 参考

- 旧实现（只读参考）：`probe/FSY-I/RAD-I/dev_protocol/net_raw/`
- 寄存器表：`probe/zjb/zjb/辐射报警仪协议命令和寄存器表0502.csv`
- 协议说明：`probe/zjb/zjb/辐射报警仪协议说明05.txt`

## 计划模块

```
Protocol/
├── frame/          帧编解码
├── register/       寄存器映射
├── transport/      TCP / CAN / LoRa / UART
└── app/            业务：0x23 实时、0x006C 历史、OTA
```
