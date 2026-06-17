# TCP 模式 OTA 详细逻辑流程

## 一、整体流程概览

```
用户选择 APP 固件
    ↓
点击"更新"按钮
    ↓
检查条件（TCP 连接、文件选择）
    ↓
发送 OTA 开始指令（写入文件大小）
    ↓
等待设备进入 STARTED 状态
    ↓
循环发送数据包（128 字节/包）
    ↓
设备校验每个数据包
    ↓
所有数据发送完成
    ↓
发送重启指令
    ↓
OTA 完成
```

---

## 二、详细流程分析

### 阶段 1：启动准备（第 3337-3414 行）

#### 1.1 条件检查
```python
# 检查 1：TCP 模式只能更新 APP 固件
if mode == "TCP" and update_type != "APP":
    log("[OTA] 警告：TCP 模式下只能更新 APP 固件")
    return

# 检查 2：TCP 必须已连接
if not self.tcp_connected:
    log("[OTA] 警告：请先在 TCP 模式下连接设备")
    return
```

#### 1.2 读取固件文件
```python
with open(file_path, 'rb') as f:
    firmware_data = f.read()
    file_size = len(firmware_data)
```

#### 1.3 发送 OTA 开始指令
```python
# 写入寄存器 200-201（REG_OTA_FILE_SIZE）
# 数据：文件大小（4 字节，小端序）
ota_start_cmd = NetRawProtocol.build_write_multi(
    addr=device_addr,
    reg=200,  # REG_OTA_FILE_SIZE
    values=[file_size & 0xFFFF, (file_size >> 16) & 0xFFFF]
)
send_data(ota_start_cmd)
```

#### 1.4 初始化 OTA 状态
```python
self.firmware_data = firmware_data          # 固件数据
self.firmware_total_size = file_size        # 总大小
self.firmware_sent_bytes = 0                # 已发送字节
self.ota_start_time = time.time()           # 开始时间
self.ota_last_state = 0                     # 初始状态
self._last_status_time = time.time()        # 最后收到状态时间
self.ota_mode = "tcp"                       # OTA 模式
self.current_update_type = "APP"            # 更新类型
self.firmware_update_active = True          # 更新标志
```

#### 1.5 禁用界面按钮
```python
_set_command_buttons_state(tk.DISABLED)
iap_update_btn.config(state=tk.DISABLED)
app_update_btn.config(state=tk.DISABLED)
iap_browse_btn.config(state=tk.DISABLED)
app_browse_btn.config(state=tk.DISABLED)
```

---

### 阶段 2：状态监听（第 3447-3495 行）

**重要**：当前代码已**关闭主动轮询**，依赖单片机主动上传状态。

#### 2.1 状态定义
```python
OTA_STATE_IDLE = 0      # 空闲
OTA_STATE_STARTED = 1   # 已开始（准备接收数据）
OTA_STATE_VERIFY = 2    # 校验中
OTA_STATE_ERROR = 3     # 错误
OTA_STATE_DONE = 4      # 完成
```

#### 2.2 状态处理逻辑

**收到 IDLE 状态 (0)**：
- 无操作，等待设备进入下一个状态

**收到 STARTED 状态 (1)**：
```python
if state == 1 and written_bytes >= self.firmware_sent_bytes:
    if written_bytes > self.firmware_sent_bytes:
        # written_bytes 增加，说明上一个包校验通过
        self.firmware_sent_bytes = written_bytes
        progress = (self.firmware_sent_bytes / self.firmware_total_size) * 100
        update_progress(progress)
        log(f"[OTA] 已发送：{firmware_sent_bytes}/{firmware_total_size} ({progress:.1f}%)")
    elif firmware_sent_bytes == 0:
        log("[OTA] 设备已进入接收状态，开始发送固件数据")
    else:
        # written_bytes == firmware_sent_bytes，重复状态上报，忽略
        return
    
    # 发送下一个数据包
    send_next_ota_packet()
```

**收到 VERIFY 状态 (2)**：
```python
elif state == 2:
    # 设备正在校验当前数据包
    # 等待设备校验完成并返回 STARTED 状态
    pass  # 静默等待
```

**收到 ERROR 状态 (3)**：
```python
if state == 3:
    log("[OTA] 设备报告错误！")
    stop_firmware_update()  # 停止 OTA
    return
```

**收到 DONE 状态 (4)**：
```python
elif state == 4:
    log("[OTA] 固件校验成功，发送重启指令...")
    reboot_command_count = 0
    send_reboot_command()  # 发送重启指令
```

---

### 阶段 3：数据包发送（第 3497-3558 行）

#### 3.1 计算数据包大小
```python
remaining = self.firmware_total_size - self.firmware_sent_bytes

# 每个包最大 128 字节（64 个寄存器 × 2 字节）
if remaining >= 128:
    packet_size = 128
elif remaining >= 4:
    # 剩余数据在 4-127 字节之间，向下取整到 4 的倍数
    packet_size = (remaining // 4) * 4
else:
    # 剩余数据不足 4 字节，按 4 字节处理
    packet_size = 4

if packet_size <= 0:
    # 所有数据发送完成
    finish_ota_update()
    return
```

#### 3.2 准备数据包
```python
start_offset = self.firmware_sent_bytes
packet_data = self.firmware_data[start_offset:start_offset + packet_size]

# 确保长度是偶数（2 字节=1 个寄存器）
if len(packet_data) % 2 != 0:
    packet_data = packet_data + b'\x00'  # 补齐到偶数
```

#### 3.3 构建写多寄存器指令
```python
# 将字节数据转换为 16 位寄存器值（小端序）
register_values = []
for i in range(0, len(packet_data), 2):
    word = packet_data[i] | (packet_data[i + 1] << 8)
    register_values.append(word)

# 写入寄存器 208 开始（REG_OTA_DATA）
ota_data_cmd = NetRawProtocol.build_write_multi(
    addr=device_addr,
    reg=208,  # REG_OTA_DATA
    values=register_values
)
```

#### 3.4 显示发送数据
```python
# 在 TX 发送框显示 HEX 数据
timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
tx_text.insert(tk.END, f"[{timestamp}]\n", 'black')
color = tx_colors[color_index % len(tx_colors)]
color_index += 1

hex_str = " ".join(f"{b:02X}" for b in ota_data_cmd)
for i in range(0, len(hex_str.split()), 16):
    line = ' '.join(hex_str.split()[i:i+16])
    tx_text.insert(tk.END, line + "\n", ('color_' + color))
tx_text.see(tk.END)
```

#### 3.5 发送数据
```python
# 通过 TCP 通道直接发送
send_ota_data(ota_data_cmd)

# 记录发送时间（用于超时检测）
packet_send_time = time.time()
```

---

### 阶段 4：完成处理（第 3560-3640 行）

#### 4.1 发送重启指令
```python
def send_reboot_command(self):
    """发送重启指令（寄存器 202-203，state=0, written_bytes=0）"""
    if self.reboot_command_count < 2:
        # 发送 2 次重启指令，确保设备收到
        reboot_cmd = NetRawProtocol.build_write_multi(
            addr=device_addr,
            reg=202,  # REG_OTA_REBOOT
            values=[0, 0]  # state=0, written_bytes=0
        )
        send_data(reboot_cmd)
        self.reboot_command_count += 1
        # 100ms 后发送第二次
        root.after(100, send_reboot_command)
    else:
        # 重启指令发送完成
        finish_ota_update()
```

#### 4.2 完成 OTA
```python
def finish_ota_update(self):
    """完成 OTA 更新"""
    log("[OTA] 固件更新完成")
    
    # 恢复按钮状态
    _set_buttons_state(True)
    iap_browse_btn.config(state=tk.NORMAL)
    app_browse_btn.config(state=tk.NORMAL)
    
    # 根据当前模式重新设置按钮状态
    _update_ota_buttons_state()
    
    # 重置 OTA 标志
    firmware_update_active = False
    app_update_active = False
```

---

## 三、关键特性

### 3.1 数据流
```
上位机                          单片机
  |                               |
  |--- OTA 开始指令 (文件大小) --->|
  |                               |
  |<-- STARTED 状态 (written=0) --|
  |                               |
  |--- 数据包 #1 (128 字节) ------->|
  |                               |
  |<-- STARTED 状态 (written=128) -|
  |                               |
  |--- 数据包 #2 (128 字节) ------->|
  |                               |
  |<-- STARTED 状态 (written=256) -|
  |                               |
  |          ...                  |
  |                               |
  |--- 数据包 #N (剩余字节) ------->|
  |                               |
  |<-- DONE 状态 -----------------|
  |                               |
  |--- 重启指令 (2 次) ----------->|
  |                               |
```

### 3.2 数据包规格
- **包大小**：128 字节（最大）
- **寄存器数量**：64 个（128 字节 ÷ 2 字节/寄存器）
- **起始寄存器**：208（REG_OTA_DATA）
- **数据格式**：小端序（Little-Endian）

### 3.3 状态机
```
IDLE (0)
  ↓
STARTED (1) ←→ VERIFY (2)  [循环直到完成]
  ↓
DONE (4)
  ↓
重启
```

### 3.4 错误处理
- **ERROR 状态**：立即停止 OTA
- **超时检测**：已关闭（依赖单片机主动上报）
- **数据校验**：由单片机完成，上位机只负责发送

---

## 四、当前代码特点

### 4.1 已关闭的功能
```python
# ❌ 已关闭：主动轮询状态
# self.root.after(200, self.poll_ota_state)

# ❌ 已关闭：超时检测
# if elapsed_ms >= 20000 and ota_last_state != 1:
#     log("[OTA] 状态超时！...")

# ❌ 已关闭：发送后轮询
# self.root.after(200, self.poll_ota_state_after_send)
```

### 4.2 依赖单片机主动上报
- ✅ 设备必须在每个阶段主动上报状态
- ✅ 上位机被动接收状态并响应
- ✅ 减少通信开销，提高效率

### 4.3 日志输出
- ✅ 简化显示：`[OTA] state=STARTED`
- ✅ 进度显示：`[OTA] 已发送：12544/713920 (1.8%)`
- ✅ 发送框显示完整 HEX 数据（每包约 10-15 行）
- ✅ 文本框限制：最多 5000 行（自动清理旧行）

---

## 五、总结

TCP 模式 OTA 的核心逻辑是：
1. **启动**：发送文件大小，等待设备进入接收状态
2. **传输**：根据设备上报的 `written_bytes` 发送数据包（128 字节/包）
3. **校验**：设备校验每个包，上位机等待校验结果
4. **完成**：所有数据发送完成后，发送重启指令

整个过程**依赖单片机主动上报状态**，上位机只负责响应和发送数据，不主动轮询。
