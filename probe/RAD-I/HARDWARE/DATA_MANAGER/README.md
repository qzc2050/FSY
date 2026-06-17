# 历史记录管理模块使用说明

## 1. 模块功能

本模块用于管理辐射剂量历史记录数据，采用 Flash 扇区存储方案，支持时间回调和时间调快功能。

## 2. 数据格式

每条记录 8 字节：
- 4 字节：Unix 时间戳（uint32_t）
- 4 字节：剂量值（uint32_t，单位：微西弗 uSv）

字符串格式：`YYYYMMDD,HHMMSS,DD.DDunit`

示例：
```
20260604,120000,01.23uSv
20260604,120500,02.34uSv
20260604,121000,03.45mSv
```

说明：
- `20260604` = 2026 年 6 月 4 日
- `120000` = 12 点 0 分 0 秒
- `01.23uSv` = 1.23 微西弗
- `03.45mSv` = 3.45 毫西弗

## 3. 配置参数

在 `hist_record_config.h` 中可配置：

```c
// 最大记录数
#define HIST_RECORD_MAX_RECORDS  300U

// 每条记录大小（字节）
#define HIST_RECORD_SIZE_BYTES   8U

// 日期时间字符串长度
#define HIST_DATE_TIME_LEN       15U

// 剂量值字符串长度
#define HIST_DOSE_VALUE_LEN      10U
```

## 4. 函数接口

### 4.1 初始化函数

```c
// 初始化历史记录管理器
int HistRecord_Init(void);

// 使用示例：
int ret = HistRecord_Init();
if (ret != 0) {
    printf("初始化失败\r\n");
}
```

### 4.2 写入函数

```c
// 写入一条历史记录
int HistRecord_Write(const char *datetime, const char *dose_value);

// 使用示例：
HistRecord_Write("20260604,120000", "01.23uSv");
HistRecord_Write("20260604,120500", "02.34mSv");
```

### 4.3 读取指定记录

```c
// 打印单条记录
int HistRecord_Print(uint16_t index);

// 读取原始数据
int HistRecord_ReadRecordRaw(uint16_t index, uint32_t *unix_ts, float *dose_uSv);

// 使用示例：
HistRecord_Print(0);  // 打印最旧一条数据
HistRecord_Print(1);  // 打印第 2 旧的数据

uint32_t ts;
float dose;
HistRecord_ReadRecordRaw(0, &ts, &dose);
```

### 4.4 读取全部数据

```c
// 读取并打印所有记录
uint16_t HistRecord_ReadAll(void);

// 使用示例：
uint16_t count = HistRecord_ReadAll();
printf("总记录数：%u\r\n", count);
```

### 4.5 其他函数

```c
// 获取有效记录数
uint16_t HistRecord_GetValidCount(void);

// 清空所有记录
int HistRecord_Clear(void);
```

### 4.6 时间调整功能（核心特性）

```c
// 时间回调处理（时间往回调整）
int HistRecord_AdjustTimeBackward(const char *new_datetime);

// 时间调快处理（时间往前调整）
int HistRecord_AdjustTimeForward(const char *new_datetime);

// 使用示例：
// 时间从 18:00 回调到 12:00
HistRecord_AdjustTimeBackward("20260604,120000");

// 时间从 12:00 调快到 20:00
HistRecord_AdjustTimeForward("20260604,200000");
```

## 5. 特性说明

### 5.1 Flash 扇区布局
- 正式扇区：存储已完成的历史记录
- 缓存扇区：正在写入的历史记录
- 临时扇区：时间回调时的缓冲扇区
- 元数据扇区：存储管理信息

### 5.2 循环覆盖
当数据达到最大记录数后，新数据会自动覆盖时间上最旧的数据。

### 5.3 时间回调
支持时间往回调整，会自动：
- 扫描所有扇区确定有效范围
- 使用临时扇区保存当前数据
- 从指定位置重新开始写入
- 保留原数据供时间调快时恢复

### 5.4 时间调快
支持时间往前调整，会自动：
- 恢复临时扇区保存的原数据
- 丢弃时间回调期间写入的数据
- 恢复到原写入位置继续写入

### 5.5 掉电保护
每次写入后都会保存元数据，确保断电后数据不丢失。

## 6. 完整使用示例

```c
#include "hist_record_app.h"

// 系统初始化完成后调用
void System_Init(void)
{
    // 初始化历史记录管理
    int ret = HistRecord_Init();
    if (ret != 0) {
        printf("初始化失败\r\n");
        return;
    }
    
    printf("有效记录数：%u\r\n", HistRecord_GetValidCount());
}

// 保存历史记录
void Save_History_Data(void)
{
    // 格式：日期时间，剂量值
    HistRecord_Write("20260604,120000", "01.23uSv");
    HistRecord_Write("20260604,120500", "02.34uSv");
}

// 查看数据
void Check_Data(void)
{
    // 读取全部数据
    HistRecord_ReadAll();
    
    // 查看最新一条
    HistRecord_Print(0);
    
    // 获取有效记录数
    uint16_t count = HistRecord_GetValidCount();
    printf("总记录数：%u\r\n", count);
}

// 时间调整示例
void Time_Adjust_Example(void)
{
    // 假设当前时间 18:00，需要回调到 12:00
    HistRecord_AdjustTimeBackward("20260604,120000");
    
    // 写入新数据（从 12:00 开始）
    HistRecord_Write("20260604,120100", "01.00uSv");
    HistRecord_Write("20260604,120200", "02.00uSv");
    
    // 发现时间错误，调快到 20:00
    HistRecord_AdjustTimeForward("20260604,200000");
    
    // 系统会自动恢复原数据，并从原位置继续写入
}
```

## 7. 注意事项

1. **时间格式**：必须严格遵守 `YYYYMMDD,HHMMSS` 格式
2. **剂量格式**：支持 uSv 和 mSv 两种单位
3. **写入频率**：频繁写入可能影响 Flash 寿命，建议合理控制写入间隔
4. **时间调整**：时间回调和调快会重新计算有效记录数，请谨慎使用

## 8. 实际应用接口

模块还提供了示例代码（见 `data_manager_example.c`）：

```c
// 保存剂量数据（带完整日期时间参数）
int Save_Dose_Data(uint8_t year, uint8_t month, uint8_t day,
                   uint8_t hour, uint8_t minute,
                   float dose_mSv);
```

## 9. 测试验证

运行完整测试：

```c
#include "data_manager_test.h"

void Test_History_Manager(void)
{
    Data_Manager_RunAllTests();  // 执行完整测试流程
}
```

测试内容包括：
- 初始化和基本写入
- 读取记录
- 读取所有记录
- 时间回调测试（未满一轮）
- 时间调快测试（恢复缓存扇区）
- 环形缓冲读取测试
- 综合场景测试

## 10. 架构说明

### 10.1 模块化架构
```
应用层（cmd.c, geiger.c 等）
    ↓ 直接调用
HistRecord_* 接口 (hist_record_app.c)
    ↓ 使用
DM_Core_* 接口 (dm_core.c)
    ↓ 使用
DM_FlashBackend_* 接口 (dm_flash_backend.c)
    ↓ 调用
W25QXX QSPI Flash 驱动
```

### 10.2 文件组织
- `hist_record_app.h/c` - 应用层 API 和实现
- `hist_record_format.h/c` - 数据格式定义和转换
- `hist_record_config.h` - 应用层配置
- `dm_core.h/c` - 核心层 API 和实现
- `dm_config.h` - 核心层配置
- `dm_flash_backend.h/c` - Flash 后端实现
- `data_manager_test.c` - 测试代码
- `data_manager_example.c` - 使用示例
