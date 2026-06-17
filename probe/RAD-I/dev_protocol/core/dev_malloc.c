/**********************************************************************************************************
 * 文件名: dev_malloc.c
 * 概  述: 设备内存管理
 * 创建时间: 2025-08-01
 * 更新时间: 2025-08-22
 * 作  者: LYJ
 * 版  本: 1.0.0
 * Copyright (c) 2025, Mr.Liu All rights reserved.
 **********************************************************************************************************/
#include "./core/dev_malloc.h"


static uint8_t dev_mempl[DEV_MEM_MALLOC_SIZE] REMAP_MEMPL_ADDRESS;      // 设备内存池
static DEV_DATA_TYPE dev_memtb[DEV_MEM_TB_SIZE] REMAP_MEMTB_ADDRESS;    // 设备内存管理表


// 设备内存管理结构体
Dev_Mem_t mem_ctrl =
{
    .init = Dev_Mem_Init,       // 内存初始化
    .usage = Dev_Mem_Usage,     // 内存使用率
    .mempl = dev_mempl,         // 内存池
    .memtb = dev_memtb,         // 内存管理状态表
    .memrdy = false,            // 内存管理未就绪
};


/********************************************************************************************
* 函数名：Dev_Mem_Init
* 描  述：设备内存管理初始化
* 输  入: 无
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Mem_Init(void)
{
    uint8_t mttsize = sizeof(DEV_DATA_TYPE);    // 获取内存池数组的类型长度(uint16_t /uint32_t)
    memset(mem_ctrl.memtb, 0, DEV_MEM_TB_SIZE * mttsize);    // 内存状态表数据清零
    mem_ctrl.memrdy = true;    // 内存管理初始化成功
}

/********************************************************************************************
* 函数名：Dev_Mem_Set
* 描  述：设备内存赋值
* 输  入: @param: *src -> 源地址
*         @param: val -> 设置值
*         @param: size -> 内存大小（单位：字节）
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Mem_Set(void *src, uint8_t val, uint32_t size)
{
    uint8_t *xs = src;

    while(size--)
        *xs++ = val;
}

/********************************************************************************************
* 函数名：Dev_Mem_Copy
* 描  述：设备内存复制
* 输  入: @param: *des -> 目标地址
*         @param: *src -> 源地址
*         @param: size -> 内存大小（单位：字节）
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Mem_Copy(void *des, void *src, uint32_t size)
{
    uint8_t *xdes = des;
    uint8_t *xsrc = src;

    while(size--)
        *xdes++ = *xsrc++;
}

/********************************************************************************************
* 函数名：Mem_Malloc
* 描  述：内存分配
* 输  入: @param: size -> 内存大小（单位：字节）
* 输  出：@retval: 0 ~ DEV_MEM_MALLOC_SIZE -> 有效内存偏移地址
*         @retval: 0XFFFFFFFF -> 无效内存偏移地址
* 调  用：内部调用
********************************************************************************************/
static uint32_t Mem_Malloc(uint32_t size)
{
    signed long offset = 0;
    uint32_t nmemb;             // 申请内存块数
    uint32_t cmemb = 0;         // 连续空内存块数
    
    if(!mem_ctrl.memrdy)        // 未初始化
        mem_ctrl.init();
    
    if(!size)    // 分配大小为 0
        return MEM_INVAILD_ADDR;

    nmemb = size / DEV_MEM_BLK_SIZE;    // 计算申请内存块数

    if(size % DEV_MEM_BLK_SIZE) 
        nmemb++;

    for(offset = DEV_MEM_TB_SIZE - 1; offset >= 0; offset--)    // 检索内存池
    {
        if(!mem_ctrl.memtb[offset])
            cmemb++;          // 累计连续空内存块数
        else
            cmemb = 0;        // 重置连续空内存块数
        
        if(cmemb == nmemb)    // 存在符合的连续空内存块数
        {
            for(uint32_t i = 0; i < nmemb; i++)    // 标记内存块已占用
                mem_ctrl.memtb[offset + i] = nmemb;
            return (offset * DEV_MEM_BLK_SIZE);    // 返回偏移地址（分配成功）
        }
    }
    
    DEV_PRINTF("内存管理 -> 内存分配 -> 内存大小：%u -> 分配失败！\r\n", size);
    return MEM_INVAILD_ADDR;    // 内存分配失败 -> 返回无效内存偏移地址
}

/********************************************************************************************
* 函数名：Mem_Release
* 描  述：内存释放
* 输  入: @param: offset -> 内存偏移地址
* 输  出：@retval: 0 -> 释放成功，1 -> 释放失败，2 -> 无效地址
* 调  用：内部调用
********************************************************************************************/
static uint8_t Mem_Release(uint32_t offset)
{
    if(!mem_ctrl.memrdy)    // 内存池未初始化
    {
        mem_ctrl.init();
        DEV_PRINTF("内存管理 -> 内存释放 -> 内存地址：%#08X -> 释放失败！\r\n", offset);
        return 1;           // 初始化失败
    }

    if(offset < DEV_MEM_MALLOC_SIZE)    // 偏移地址于内存池内
    {
        uint32_t index = offset / DEV_MEM_BLK_SIZE;    // 偏移地址所在内存块索引
        uint32_t nmemb = mem_ctrl.memtb[index];        // 内存块数量

        for(uint32_t i = 0; i < nmemb; i++)            // 内存块释放
            mem_ctrl.memtb[index + i] = 0;

        memset(dev_mempl + offset, 0, nmemb * DEV_MEM_BLK_SIZE);
        // DEV_PRINTF("内存管理 -> 内存释放 -> 内存地址：%#08X -> 释放成功！\r\n", offset);
        return 0;
    }
    else
    {
        DEV_PRINTF("内存管理 -> 内存释放 -> 内存地址：%#08X -> 无效地址！\r\n", offset);
        return 2;    // 无效地址
    }
}

/********************************************************************************************
* 函数名：Dev_Mem_Release
* 描  述：设备内存释放
* 输  入: @param: *ptr -> 内存释放首地址
* 输  出：无
* 调  用：外部调用
********************************************************************************************/
void Dev_Mem_Release(void *ptr)
{
    if(ptr == NULL)    // 无效地址
        return;
    Mem_Release((uint32_t)ptr - (uint32_t)mem_ctrl.mempl);    // 内存释放
}

/********************************************************************************************
* 函数名：Dev_Mem_Malloc
* 描  述：设备内存分配
* 输  入: @param: size -> 内存大小（单位：字节）
* 输  出：@retval: 0 ~ DEV_MEM_MALLOC_SIZE -> 有效内存偏移地址
*         @retval: MEM_INVAILD_ADDR -> 无效内存偏移地址
* 调  用：外部调用
********************************************************************************************/
void *Dev_Mem_Malloc(uint32_t size)
{
    uint32_t offset = Mem_Malloc(size);
    return (offset == MEM_INVAILD_ADDR) ? NULL : (void *)((uint32_t)mem_ctrl.mempl + offset);
}

/********************************************************************************************
* 函数名：Dev_Mem_Realloc
* 描  述：设备内存重分配
* 输  入: @param: *ptr -> 释放内存首地址
*         @param: size -> 内存大小（单位：字节）
* 输  出：@retval: 0 ~ DEV_MEM_MALLOC_SIZE -> 有效内存偏移地址
* 调  用：外部调用
********************************************************************************************/
void *Dev_Mem_Realloc(void *ptr, uint32_t size)
{
    uint32_t offset = Mem_Malloc(size);

    if(offset == MEM_INVAILD_ADDR)    // 内存重分配失败
    {
        DEV_PRINTF("内存管理 -> 内存重分配 -> 分配失败！");
        return NULL;    // 返回空
    }
    else    // 内存重分配成功，返回新内存首地址
    {
        if(ptr)
        {
            memcpy((void *)((uint32_t)mem_ctrl.mempl + offset), ptr, size);    // 数据转移
            Dev_Mem_Release(ptr);    // 释放内存
        }
        return (void *)((uint32_t)mem_ctrl.mempl + offset);    // 返回新内存首地址
    }
}

/********************************************************************************************
* 函数名：Dev_Mem_Usage
* 描  述：获取设备内存使用率
* 输  入: 无
* 输  出：@retval: 内存使用率（0.0% ~ 100.0%）
* 调  用：外部调用
********************************************************************************************/
float Dev_Mem_Usage(void)
{
    float used = 0;

    for(uint32_t i = 0; i < DEV_MEM_TB_SIZE; i++)
        if(mem_ctrl.memtb[i])
            used++;

    return (used / (float)DEV_MEM_TB_SIZE * 100.0f);
}

/********************************************************************************************
* 函数名：Dev_Mem_Pool_Addr
* 描  述：获取设备内存池地址
* 输  入: 无
* 输  出：@retval: 设备内存池地址
* 调  用：外部调用
********************************************************************************************/
uint32_t Dev_Mem_Pool_Addr(void)
{
    return (uint32_t)&dev_mempl;
}

/********************************************************************************************
* 函数名：Dev_Mem_Table_Addr
* 描  述：获取设备内存管理表地址
* 输  入: 无
* 输  出：@retval: 设备内存管理表地址
* 调  用：外部调用
********************************************************************************************/
uint32_t Dev_Mem_Table_Addr(void)
{
    return (uint32_t)&dev_memtb;
}
