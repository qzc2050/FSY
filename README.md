# NetShield

联盾环境辐射监测系统控制主机（Android 11 / RK3568 工控平板）

## 打开工程

1. Android Studio → **Open** → 选择本目录 `Fsy`
2. 等待 Gradle Sync 完成
3. 连接平板或模拟器（API 30+，建议横屏）→ Run

## 工程结构

```
app/src/main/java/com/raydose/netshield/
├── MainActivity.kt          # 入口
├── model/                   # UI 状态模型
├── net/                     # 协议层（自 testuart 迁移）
└── ui/
    ├── components/          # 共用 Composable
    ├── home/HomeScreen.kt   # 主页骨架
    └── theme/               # NetShield 主题
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
- [x] **设置 · 探头管理**（image19 首版：添加/删除/分页表单、寄存器读写规则）— **本阶段暂告一段落**
- [ ] **多探头** 联调测试（下一步）
- [ ] 设置其余 Tab、音乐 / 相册 / 文件管理等子页

详见 [`docs/开发进度.md`](docs/开发进度.md)。
