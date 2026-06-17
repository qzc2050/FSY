#include "hist_record_app.h"
#include <stdio.h>
#include <string.h>

/*==========================================================================
 * 历史记录管理模块测试代码
 * 包括基础功能测试和时间调整功能测试
 *==========================================================================*/

/** 测试通过/失败宏 */
#define TEST_PASS()  do { printf("[PASS] %s\r\n", __func__); return 0; } while(0)
#define TEST_FAIL(msg) do { printf("[FAIL] %s: %s\r\n", __func__, msg); return -1; } while(0)

/*==========================================================================
 * 基础功能测试
 *==========================================================================*/

/**
 * 测试 1：初始化和基本写入
 */
int test_basic_write(void)
{
    int ret;
    uint16_t count;
    
    printf("\r\n===== 测试 1：初始化和基本写入 =====\r\n");
    
    /* 初始化 */
    ret = HistRecord_Init();
    if (ret != 0) {
        TEST_FAIL("初始化失败");
    }
    printf("[OK] 初始化成功\r\n");
    
    /* 写入 3 条记录 */
    ret = HistRecord_Write("20260604,120000", 12300);  /* 01.23uSv = 12300 uSv */
    if (ret != 0) {
        TEST_FAIL("写入第 1 条失败");
    }
    
    ret = HistRecord_Write("20260604,120500", 23400);  /* 02.34uSv = 23400 uSv */
    if (ret != 0) {
        TEST_FAIL("写入第 2 条失败");
    }
    
    ret = HistRecord_Write("20260604,121000", 34500);  /* 03.45uSv = 34500 uSv */
    if (ret != 0) {
        TEST_FAIL("写入第 3 条失败");
    }
    
    /* 检查有效记录数 */
    count = HistRecord_GetValidCount();
    if (count != 3) {
        printf("错误：期望 3 条，实际%u条\r\n", count);
        TEST_FAIL("有效记录数不正确");
    }
    
    printf("[OK] 写入 3 条记录成功，有效记录数=%u\r\n", count);
    TEST_PASS();
}

/**
 * 测试 2：读取记录
 */
int test_read_records(void)
{
    int ret;
    uint32_t ts;
    float dose;
    
    printf("\r\n===== 测试 2：读取记录 =====\r\n");
    
    /* 先初始化并写入 */
    HistRecord_Clear();
    HistRecord_Write("20260604,130000", 50000);  /* 05.00uSv */
    HistRecord_Write("20260604,130500", 100000); /* 10.00uSv */
    HistRecord_Write("20260604,131000", 150000); /* 15.00uSv */
    
    /* 读取第 1 条 */
    ret = HistRecord_ReadRecordRaw(0, &ts, &dose);
    if (ret != 0) {
        TEST_FAIL("读取第 1 条失败");
    }
    printf("[OK] 第 1 条：ts=%lu, dose=%.2f\r\n", ts, dose);
    
    /* 读取第 2 条 */
    ret = HistRecord_ReadRecordRaw(1, &ts, &dose);
    if (ret != 0) {
        TEST_FAIL("读取第 2 条失败");
    }
    printf("[OK] 第 2 条：ts=%lu, dose=%.2f\r\n", ts, dose);
    
    /* 读取第 3 条 */
    ret = HistRecord_ReadRecordRaw(2, &ts, &dose);
    if (ret != 0) {
        TEST_FAIL("读取第 3 条失败");
    }
    printf("[OK] 第 3 条：ts=%lu, dose=%.2f\r\n", ts, dose);
    
    TEST_PASS();
}

/**
 * 测试 3：读取所有记录
 */
int test_read_all(void)
{
    printf("\r\n===== 测试 3：读取所有记录 =====\r\n");
    
    /* 先初始化并写入 */
    HistRecord_Clear();
    HistRecord_Write("20260604,140000", 10000);  /* 01.00uSv */
    HistRecord_Write("20260604,140500", 20000);  /* 02.00uSv */
    HistRecord_Write("20260604,141000", 30000);  /* 03.00uSv */
    
    /* 读取所有 */
    HistRecord_ReadAll();
    
    TEST_PASS();
}

/*==========================================================================
 * 时间调整功能测试
 *==========================================================================*/

/**
 * 测试 4：时间回调（未满一轮）
 */
int test_time_backward_partial(void)
{
    int ret;
    uint16_t count_before, count_after;
    
    printf("\r\n===== 测试 4：时间回调（未满一轮） =====\r\n");
    
    /* 初始化并写入一些数据 */
    HistRecord_Clear();
    
    printf("写入 10 条记录（12:00-12:50）...\r\n");
    for (int i = 0; i < 10; i++) {
        char datetime[20];
        sprintf(datetime, "20260604,12%02d00", i);
        HistRecord_Write(datetime, 10000);  /* 01.00uSv */
    }
    
    count_before = HistRecord_GetValidCount();
    printf("回调前有效记录数：%u\r\n", count_before);
    
    /* 时间回调到 12:05 */
    printf("时间回调到 12:05...\r\n");
    ret = HistRecord_AdjustTimeBackward("20260604,120500");
    if (ret != 0) {
        TEST_FAIL("时间回调失败");
    }
    
    count_after = HistRecord_GetValidCount();
    printf("回调后有效记录数：%u\r\n", count_after);
    
    /* 写入新数据 */
    printf("写入新数据（12:05 之后）...\r\n");
    HistRecord_Write("20260604,120600", 20000);  /* 02.00uSv */
    HistRecord_Write("20260604,120700", 30000);  /* 03.00uSv */
    
    /* 读取所有 */
    HistRecord_ReadAll();
    
    TEST_PASS();
}

/**
 * 测试 5：时间调快（恢复缓存扇区）
 */
int test_time_forward(void)
{
    int ret;
    uint16_t count;
    
    printf("\r\n===== 测试 5：时间调快（恢复缓存扇区） =====\r\n");
    
    /* 先初始化并写入 */
    HistRecord_Clear();
    
    printf("写入 20 条记录（13:00-14:55）...\r\n");
    for (int i = 0; i < 20; i++) {
        char datetime[20];
        sprintf(datetime, "20260604,13%02d00", i);
        HistRecord_Write(datetime, 10000);  /* 01.00uSv */
    }
    
    count = HistRecord_GetValidCount();
    printf("回调前有效记录数：%u\r\n", count);
    
    /* 时间回调到 13:10 */
    printf("时间回调到 13:10...\r\n");
    ret = HistRecord_AdjustTimeBackward("20260604,131000");
    if (ret != 0) {
        TEST_FAIL("时间回调失败");
    }
    
    count = HistRecord_GetValidCount();
    printf("回调后有效记录数：%u\r\n", count);
    
    /* 写入少量新数据 */
    printf("写入 2 条新数据...\r\n");
    HistRecord_Write("20260604,131100", 20000);  /* 02.00uSv */
    HistRecord_Write("20260604,131200", 30000);  /* 03.00uSv */
    
    /* 时间调快到 14:00 */
    printf("时间调快到 14:00...\r\n");
    ret = HistRecord_AdjustTimeForward("20260604,140000");
    if (ret != 0) {
        TEST_FAIL("时间调快失败");
    }
    
    count = HistRecord_GetValidCount();
    printf("调快后有效记录数：%u\r\n", count);
    
    /* 读取所有 */
    HistRecord_ReadAll();
    
    TEST_PASS();
}

/**
 * 测试 6：环形缓冲读取
 */
int test_ring_buffer_read(void)
{
    printf("\r\n===== 测试 6：环形缓冲读取 =====\r\n");
    
    /* 初始化并写入较多数据 */
    HistRecord_Clear();
    
    printf("写入 50 条记录，测试环形缓冲...\r\n");
    for (int i = 0; i < 50; i++) {
        char datetime[20];
        sprintf(datetime, "20260604,%02d%02d00", 10 + i / 60, i % 60);
        HistRecord_Write(datetime, 10000);  /* 01.00uSv */
    }
    
    /* 读取所有 */
    HistRecord_ReadAll();
    
    TEST_PASS();
}

/*==========================================================================
 * 综合测试
 *==========================================================================*/

/**
 * 测试 7：综合场景测试
 */
int test_comprehensive(void)
{
    int ret;
    
    printf("\r\n===== 测试 7：综合场景测试 =====\r\n");
    
    /* 1. 初始化 */
    HistRecord_Clear();
    printf("1. 初始化完成\r\n");
    
    /* 2. 写入 100 条记录 */
    printf("2. 写入 100 条记录...\r\n");
    for (int i = 0; i < 100; i++) {
        char datetime[20];
        sprintf(datetime, "20260604,%02d%02d00", 8 + i / 60, i % 60);
        HistRecord_Write(datetime, 10000);  /* 01.00uSv */
    }
    printf("   有效记录数：%u\r\n", Data_5Min_GetValidCount());
    
    /* 3. 时间回调到 09:00 */
    printf("3. 时间回调到 09:00...\r\n");
    ret = HistRecord_AdjustTimeBackward("20260604,090000");
    if (ret != 0) {
        TEST_FAIL("时间回调失败");
    }
    printf("   有效记录数：%u\r\n", Data_5Min_GetValidCount());
    
    /* 4. 写入 20 条新数据 */
    printf("4. 写入 20 条新数据...\r\n");
    for (int i = 0; i < 20; i++) {
        char datetime[20];
        sprintf(datetime, "20260604,09%02d00", i);
        HistRecord_Write(datetime, 20000);  /* 02.00uSv */
    }
    printf("   有效记录数：%u\r\n", Data_5Min_GetValidCount());
    
    /* 5. 时间调快到 12:00 */
    printf("5. 时间调快到 12:00...\r\n");
    ret = HistRecord_AdjustTimeForward("20260604,120000");
    if (ret != 0) {
        TEST_FAIL("时间调快失败");
    }
    printf("   有效记录数：%u\r\n", Data_5Min_GetValidCount());
    
    /* 6. 读取所有 */
    printf("6. 读取所有记录...\r\n");
    HistRecord_ReadAll();
    
    TEST_PASS();
}

/*==========================================================================
 * 测试入口
 *==========================================================================*/

/**
 * 运行所有测试
 */
void Data_Manager_RunAllTests(void)
{
    printf("\r\n");
    printf("╔══════════════════════════════════════════════════════╗\r\n");
    printf("║       历史记录管理模块测试                           ║\r\n");
    printf("╚══════════════════════════════════════════════════════╝\r\n");
    
    /* 基础功能测试 */
    test_basic_write();
    test_read_records();
    test_read_all();
    
    /* 时间调整功能测试 */
    test_time_backward_partial();
    test_time_forward();
    test_ring_buffer_read();
    
    /* 综合测试 */
    test_comprehensive();
    
    printf("\r\n");
    printf("╔══════════════════════════════════════════════════════╗\r\n");
    printf("║                  所有测试完成                        ║\r\n");
    printf("╚══════════════════════════════════════════════════════╝\r\n");
}
