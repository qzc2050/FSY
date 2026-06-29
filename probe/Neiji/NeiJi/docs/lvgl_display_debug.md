# NeiJi LVGL 显示调试记录

> **工程**：`probe/Neiji/NeiJi`  
> **硬件**：STM32H743 + GC9503V RGB(LTDC) + 外部 SDRAM @ `0xC0000000`  
> **参考**：`probe/FSY-I/RAD-I`（同板型，LVGL 已量产）  
> **逻辑分辨率**：854×480（横屏 UI）  
> **物理帧缓冲**：480×854 RGB565 @ `0xC0000000`（90° 旋转 + DMA2D blit）

---

## 1. 历史问题（2026-06 diag=1～12）

**现象**：白字闪灰、整屏变白、LTDC FUIF。

**根因**：FMC/SDRAM 带宽不足 → LTDC FIFO 欠载（`ISR bit1 = FUIF`）。

**修复（对齐 RAD-I，diag=12 基线）**：

1. `PLL1Q = 5`，FMC @ PLL1Q，SDCLK ≈ 96 MHz  
2. AXI 仲裁：`INI6_READ_QOS=15`，`TARG5 READ_ISS_OVERRIDE`，`INI3_WRITE_QOS=0`  
3. SDRAM refresh=729，tWR=2  

详见下文 diag=1～12 迭代表（第二节）。

---

## 2. 竖条 / 闪动排查（2026-06 diag=8～14）

### 2.1 现象（完整 UI 上线后）

| 现象 | 描述 |
|------|------|
| 整屏轻微闪动 | 浅灰/白底区域 shimmer |
| 右侧淡淡竖条 | PM2.5、铃铛、辐射转轮附近最明显 |
| 严重白屏 | diag=12 已解决；`ISR=0x0` 无 FUIF |

### 2.2 已排除（逐项关闭验证）

| 开关关闭项 | 竖条/闪动 | 结论 |
|------------|-----------|------|
| `NEIJI_UI_LIVE_REFRESH`（1s 改 label） | 仍有 | ❌ 非主因 |
| `NEIJI_UI_RADIATION_SPIN`（转轮动画） | 仍有 | ❌ 非主因 |
| `NEIJI_WS2812_ENABLE`（灯带 DMA） | 仍有 | ❌ 非主因 |

**结论**：问题在 **显示管线**（LTDC + SDRAM + Cache + 90° 局部 flush），与传感器/UI 动画/WS2812 无关。

### 2.3 有效修复（当前保留，diag=14）

| 措施 | 文件 | 作用 |
|------|------|------|
| **帧缓冲 MPU non-cache（2MB）** | `Core/Src/main.c` | LTDC/DMA2D 直访 SDRAM，减轻 cache 竖条 |
| **VSYNC 同步 flush** | `lcd_rgb.c` | ⚠️ **默认关**：partial 模式下每 dirty 区 wait 一次 VSYNC（~16ms），动画+1s 刷新时 UI 极慢、灯带/转轮卡顿、传感器 >5s 未更新判离线 |
| **DMA2D 写后逐行 clean + 2px 扩边** | `lcd_rgb.c` | 局部脏区 cache 一致性 |
| **背光 PWM ~20kHz** | `Core/Src/tim.c`（TIM12 PSC 11） | 减轻亮度微闪 |
| **面板不透明预混色 `#545454`** | `ui_Main_Interface.c` | 视觉 ≈ 原 `bg_opa=60` 半透明白；**不可恢复真半透明**（会复现竖条） |

串口验证（diag=9）：`[ltdc] poll ... ISR=0x0` 持续，**无 FUIF**。

### 2.4 已知遗留（暂不处理，后续再改）

- **屏幕右侧仍有轻微竖条纹**（肉眼不仔细看不明显）  
- **真半透明面板**（`#FFFFFF` + `bg_opa=60`）与 **90° 旋转 partial flush** 不兼容，恢复即复现竖条  
- 后续可选方向：整屏 `full_refresh`、VSYNC 双缓冲、帧缓冲放内部 SRAM、或换 flush 路径

---

## 3. 配置开关（`Core/Inc/main.h`）

| 宏 | diag=14 默认 | 说明 |
|----|-------------|------|
| `NEIJI_DIAG_BUILD` | 14 | 串口启动行 `diag=N` 对版 |
| `NEIJI_UI_LIVE_REFRESH` | **1** | 主界面 1s 刷新传感器/剂量/时间 |
| `NEIJI_UI_RADIATION_SPIN` | **1** | 辐射图标转轮 |
| `NEIJI_WS2812_ENABLE` | **1** | WS2812 灯带任务 |
| `NEIJI_BEEP_PROBE` | **0** | 上电蜂鸣探针（硬件已确认） |
| `NEIJI_LTDC_FB_NOCACHE` | 1 | 帧缓冲 non-cache MPU |
| `NEIJI_DISP_VSYNC_FLUSH` | **0** | VSYNC 同步（partial 下勿开，见 2.3） |
| `NEIJI_DISP_FLUSH_PAD` | 2 | 物理行 clean 扩边（像素） |
| `NEIJI_LTDC_DIAG` | 0 | 周期 `[ltdc]` 串口（调试开 1） |

面板预混色定义：`Hardware/lcd/lcd_rgb.h` → `LCD_UI_PANEL_BLEND888`（`#545454`）。

---

## 4. diag 版本迭代表（完整）

| diag | 主要改动 | 屏现象 |
|------|----------|--------|
| 1～5 | LVGL 移植、MPU | 白字闪灰 |
| 6～7 | 测试色条、full_refresh | 短暂正确→全白 |
| **8** | LTDC 探针 | **FUIF → 全白** |
| 9 | AXI READ_ISS | 仍白屏 |
| 10 | 误改 PLL1Q=2 | 竖条抖动 |
| 11 | 恢复 FMC@PLL2 | 又白屏 |
| **12** | PLLQ=5 + FMC@PLL | ✅ 稳定清晰（step2 基线） |
| 9～11 | 竖条/闪动排查 + nocache | ISR=0，闪动减轻 |
| 10 | 不透明面板 `#A6A6A6` | 竖条无，色不对 |
| 12 | 恢复半透明 | 竖条复现 |
| 13 | 预混 `#545454` | 竖条无，色接近原设计 |
| **11** | VSYNC + 20kHz 背光 | 闪动难察觉 |
| **14** | **恢复 UI/WS2812/转轮 + 保留显示修复** | **当前发布态** |
| **15** | **关闭 VSYNC**（14 开启后整机变慢/传感器离线） | 功能速度恢复正常 |

---

## 5. 代码锚点

| 文件 | 内容 |
|------|------|
| `Core/Inc/main.h` | 上述 `NEIJI_*` 开关 |
| `Core/Src/main.c` | MPU：2MB nocache + 32MB cacheable SDRAM |
| `Core/Src/fmc.c` | FMC@PLL、AXI 仲裁 |
| `Core/Src/tim.c` | TIM12 背光/蜂鸣 PWM |
| `Core/Src/stm32h7xx_it.c` | `LTDC_IRQHandler` |
| `Hardware/lcd/lcd_rgb.c` | VSYNC、DMA2D、逐行 clean |
| `LVGL/src/porting/lv_port_disp.c` | partial flush + 旋转 blit |
| `LVGL/app/screens/ui_Main_Interface.c` | `neiji_style_sensor_panel()` |
| `App/ui_task.c` | LVGL 5ms tick；可选 `[ltdc]` 探针 |

**内存 map**：

```
0xC0000000  ltdc_lcd_framebuf   480×854×2  (MPU non-cache 2MB)
0xC00C8280  lcd_rotate_buf
0xC0190500  lvgl buf1/buf2
```

---

## 6. 参考资料

- [ST Community: READ_ISS_OVERRIDE 修复 LTDC glitch](https://community.st.com/t5/stm32-mcus-products/fixing-ltdc-glitch-by-setting-bit-read-iss-override-in-axi-targx/td-p/752349)
- 参考工程：`probe/FSY-I/RAD-I`

---

*文档版本：diag=15 · 2026-06 · 右侧轻微竖条已知遗留*
