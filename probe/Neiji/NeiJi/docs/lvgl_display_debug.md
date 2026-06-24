# NeiJi LVGL 显示调试记录

> **工程**：`probe/Neiji/NeiJi`  
> **硬件**：STM32H743 + GC9503V RGB(LTDC) + 外部 SDRAM @ `0xC0000000`  
> **参考**：`probe/FSY-I/RAD-I`（同板型，LVGL 已量产）  
> **调试周期**：2026-06（diag=1～12）  
> **稳定版本**：**diag=12**（2026-06-24 确认：画面稳定清晰）

---

## 1. 现象与目标

| 阶段 | 现象 |
|------|------|
| 初始 | LVGL step1 能画字，**白字闪一下变灰/消失** |
| diag=6+ | 加顶白/底红物理色条后，**短暂正确再整屏变白** |
| 最终 (diag=12) | 灰底 + 白字 + 测试色条 **稳定清晰**，网络/DHCP 并行无白屏 |

**逻辑分辨率**：854×480（横屏 UI）  
**物理帧缓冲**：480×854 RGB565 @ `0xC0000000`（90° 旋转 + DMA2D blit）

---

## 2. 根因（最终结论）

**主因：SDRAM（FMC）时钟带宽不足 → LTDC FIFO 欠载（FUIF）**

- LTDC 以 ~60Hz 扫描全屏，持续从 SDRAM 读帧缓冲（约 **49 MB/s**）
- NeiJi 原配置 **FMC@PLL2**，SDCLK ≈ **50 MHz**，有效读带宽处于临界，略有余载即 **ISR bit1 = FUIF**
- FUIF 后 LTDC 重复/错读行数据 → **屏上全白**（`0xFFFF`），而 **SDRAM 内数据仍正确**（探针 Invalidate 后读回正常）

**次因 / 误判路径**：

| 曾怀疑 | 结论 |
|--------|------|
| Cache Clean 盖掉 LTGL 数据 | ❌ Invalidate 后 RAM 仍正确 |
| CFBAR 地址错误 | ❌ 寄存器与 `0xC0000000` 一致 |
| 字体/旋转逻辑错误 | ❌ `[draw]` 与 `[fb]` 一致 |
| diag=10 改 FMC@PLL1Q=2 | ❌ SDCLK≈480MHz，**时序越界** → 竖条抖动、SDRAM 自检失败 |

**最终修复（对齐 RAD-I）**：

1. **PLL1Q = 5**（原 NeiJi 为 2；RAD-I 亦为 5）
2. **FMC 时钟源 = PLL1Q**（`RCC_FMCCLKSOURCE_PLL`），SDCLK ≈ **96 MHz**
3. **AXI 仲裁**：`GPV->AXI_INI6_READ_QOS=15`，`GPV->AXI_TARG5_FN_MOD_ISS_BM |= READ_ISS_OVERRIDE`，`GPV->AXI_INI3_WRITE_QOS=0`（DMA2D 写帧缓冲时不阻塞 LTDC 读）
4. **MPU**：SDRAM **cacheable + bufferable**（减轻 CPU 与 LTDC 的 FMC 争用；DMA2D 写后仍 Clean）
5. **SDRAM 刷新计数 = 729**（与 RAD-I 一致）
6. **FMC WriteRecoveryTime = 2**（与 RAD-I 一致）

---

## 3. diag 版本迭代表

通过串口 `[ui] ready diag=N` / 启动行 `diag=N` 确认烧录版本。

| diag | 主要改动 | ISR/FUIF | 屏现象 | 备注 |
|------|----------|----------|--------|------|
| 1～5 | LVGL 移植、MPU、Cache 探针 | 未系统记录 | 白字闪灰 | 排除字体-only |
| 6～7 | 顶白/底红 DMA2D 色条、full_refresh | FUIF | 短暂正确→全白 | 定位到 LTDC 侧 |
| **8** | LTDC 寄存器探针、`LCD_LtdcLogState` | **ISR=0x2 持续 FUIF** | 全白，RAM 正确 | **锁定 FIFO 欠载** |
| **9** | AXI TARG5 READ_ISS + INI6 QoS | now=0，t+1s FUIF | 仍白屏 | AXI 部分有效 |
| **10** | FMC 误改 PLL1Q=2 + INI3 wrQoS=0 | **ISR=0** | 可见但竖条抖动 | SDRAM 自检 FAIL |
| **11** | 恢复 FMC@PLL2 | t+1s FUIF | 又白屏 | 带宽仍不够 |
| **12** | **PLLQ=5 + FMC@PLL + cacheable MPU** | **ISR=0 持续** | **稳定清晰** | ✅ **发布基线** |

---

## 4. 关键探针说明（调试期）

| 标签 | 来源 | 含义 |
|------|------|------|
| `[draw]` | `lv_port_disp.c` | LVGL 逻辑缓冲区内 ink/white 像素 |
| `[fb]` | `ui_task.c` | 物理帧缓冲采样（需 `LCD_InvalidateFramebuf`） |
| `[ltdc]` | `lcd_rgb.c` | CFBAR/CFBLR/CFBLNBR/WHPCR/WVPCR/**ISR** |
| `[test]` | `ui_task.c` | DMA2D 物理色条像素抽检 |
| **ISR=0x2** | LTDC->ISR bit1 | **FUIF = FIFO Underrun** |

diag=12 稳定后，step1 仍保留轻量 `[ltdc]` 周期日志；step2 将移除测试色条与诊断探针。

---

## 5. 最终代码锚点（diag=12）

| 文件 | 内容 |
|------|------|
| `Core/Inc/main.h` | `NEIJI_DIAG_BUILD 12U` |
| `Core/Src/main.c` | `PLLQ=5`；MPU SDRAM cacheable；`FMC_ConfigLtdcSdramArbitration()` ×2 |
| `Core/Src/fmc.c` | FMC@PLL；`FMC_ConfigLtdcSdramArbitration()`；refresh=729；tWR=2 |
| `Core/Inc/fmc.h` | 声明 `FMC_ConfigLtdcSdramArbitration` |
| `Hardware/lcd/lcd_rgb.c` | DMA2D ISR Clean；`LCD_LtdcLogState`（含 FUIF 后缀） |
| `LVGL/src/porting/lv_port_disp.c` | 旋转 flush；step1 单缓冲 + full_refresh |
| `App/ui_task.c` | step1 最小 UI + 一次性探针 + ltdc-only 周期日志 |
| `MDK-ARM/NeiJi/NeiJi.sct` | SDRAM @ `0xC0000000` |

**内存 map（典型）**：

```
0xC0000000  ltdc_lcd_framebuf   480×854×2
0xC00C8280  lcd_rotate_buf
0xC0190500  lvgl buf1
```

---

## 6. 与 RAD-I 差异（修复后）

| 项 | RAD-I | NeiJi (diag=12) |
|----|-------|-----------------|
| FMC 时钟 | PLL1Q (Q=5) | ✅ 已对齐 |
| AXI READ_ISS | 社区方案（若未显式写则靠带宽裕量） | ✅ 显式配置 |
| MPU SDRAM | 无专用 region / cacheable | ✅ cacheable region |
| LVGL 缓冲 | 双缓冲 partial | step1 单缓冲 full_refresh（待 step2） |

---

## 7. 后续 step2（未做）

- [ ] 去掉顶白/底红测试色条与 `[fb]`/`[test]` 探针
- [ ] 恢复 `lv_timer_handler` 主循环
- [ ] `lv_port_disp` 对齐 RAD-I 双缓冲、去掉 `full_refresh`
- [ ] `NEIJI_DIAG_BUILD` 归零或改为正式版本号

---

## 8. 参考资料

- ST Community：[Fixing LTDC Glitch by READ_ISS_OVERRIDE in AXI_TARG5_FN_MOD_ISS_BM](https://community.st.com/t5/stm32-mcus-products/fixing-ltdc-glitch-by-setting-bit-read-iss-override-in-axi-targx/td-p/752349)
- 同仓库参考工程：`probe/FSY-I/RAD-I/Core/Src/fmc.c`（PLLQ=5，FMC@PLL）

---

*文档版本：diag=12 stable · 2026-06-24*
