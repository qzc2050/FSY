# Raylink

瑞联区域辐射监测系统控制主机（Android 11 / RK3568 工控平板）

## 产品型号

| 类别 | 型号 | 说明 |
|------|------|------|
| 主机 | RKM10 / RKM13 | 十寸 / 十三寸 |
| 内机 | RK100P / RK100D / RK100N | 全功能有屏 / 无屏有空气成分 / 仅网络链路 |
| 物联网盒子 | RK-N | 仅网络 |

## 打开工程

1. Android Studio → **Open** → 选择本目录 `Fsy`
2. 等待 Gradle Sync 完成
3. 连接平板或模拟器（API 30+，建议横屏）→ Run

## 工程结构

```
app/src/main/java/com/raydose/raylink/
├── MainActivity.kt          # 入口
├── model/                   # UI 状态模型
├── net/                     # 协议层（自 testuart 迁移）
└── ui/
    ├── components/          # 共用 Composable
    ├── home/HomeScreen.kt   # 主页骨架
    └── theme/               # Raylink 主题
```

## 文档

- 需求对照：`docs/截图与需求对照表.md`
- 协议 CSV：`程序更新OTA协议_当前实现.csv`
- UI 参考图：`docs/screenshots/`

## 模拟器联调（可选）

```bash
python tools/fsy_tcp_slave_simulator.py
```

## 当前进度

- [x] 工程骨架、横屏锁定、minSdk 30
- [x] 协议层迁移（TCP / 组播 / OTA 帧编解码）
- [x] 主页 UI + 组播发现 / TCP 0x23 → 探头卡片实时数据
- [x] 主页留言栏点击轻量上弹留言列表（同宽、居中、无标题；不触发布局上跳）
- [x] **设置 · 探头管理**（image19 首版：添加/删除/分页表单、寄存器读写规则）
- [x] 设置其余 Tab 首版（显示与声音 / 网络 / 时间 / 关于）
- [x] 音乐播放（含最小化后台播 / 关闭停播）
- [x] 电子相册首版（图片选择 + 留言管理）
- [x] 文件管理首版（查看 / 复制 / 移动 / 删除 / 新建文件夹 / 重命名）
- [x] U 盘访问优化（root 工控平板优先直连，无 root 回退 SAF）
- [x] 主机连通状态 + 7688 WiFi 拉取首版；固件报警阈值已调试完成
- [ ] **本轮迭代主线（优先）**：**ZJB OTA** → 多探头联调 → 5min 真数据闭环 → App 内 APK 更新 → LoRa 三端联调

详见 [`docs/开发进度.md`](docs/开发进度.md)（文首「当前迭代主线」）。
