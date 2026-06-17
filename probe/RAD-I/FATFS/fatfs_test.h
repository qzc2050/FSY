#ifndef __FATFS_TEST_H
#define __FATFS_TEST_H 			   
#include "ff.h"
#include <stdint.h>
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7 开发板
//FATFS 测试代码 (集中管理版本)
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2018/8/2
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
//////////////////////////////////////////////////////////////////////////////////  

//测试功能开关配置
#ifndef FATFS_RW_AUTO_MKFS
#define FATFS_RW_AUTO_MKFS 0
#endif

#ifndef FATFS_READ123_AUTO_MKFS
#define FATFS_READ123_AUTO_MKFS 0
#endif

#ifndef RUN_FATFS_RW_TEST
#define RUN_FATFS_RW_TEST  0
#endif

#ifndef RUN_FATFS_READ_123_TEST
#define RUN_FATFS_READ_123_TEST  0
#endif

#ifndef RUN_FATFS_CREATE_CSV_TEST
#define RUN_FATFS_CREATE_CSV_TEST  0
#endif

#if RUN_FATFS_RW_TEST || RUN_FATFS_READ_123_TEST || RUN_FATFS_CREATE_CSV_TEST
#define RUN_FATFS_TESTS 1
#endif



/**
 * @brief  初始化 exfuns（分配 fs[0..]、file 等）。使用逻辑盘 1: 前必须成功调用一次。
 */
FRESULT fatfs_ensure_exfuns(void);

/**
 * @brief  MCU 本地访问 1: 前调用：若本模块记录为未挂载则 f_mount(fs[1],"1:",1)
 */
FRESULT fatfs_vol1_ensure_mounted(void);

/**
 * @brief  释放 FatFs 对 1: 的挂载（例如即将由 USB MSC 独占 SPI Flash）
 */
FRESULT fatfs_vol1_unmount(void);

/**
 * @brief  是否与 fatfs_vol1_ensure_mounted / fatfs_vol1_unmount 成对记录的一致（1=已挂载）
 */
int fatfs_vol1_is_mounted(void);

/**
 * @brief  创建 FatFs 互斥锁（FF_FS_REENTRANT=0 时必须，在首次 DataManager 前调用）
 */
void fatfs_fs_mutex_init(void);

void fatfs_fs_lock(void);
void fatfs_fs_unlock(void);

/**
 * @brief  运行所有启用的 FATFS 测试
 * @param  None
 * @retval 测试结果统计：返回执行的测试数量，负数表示错误
 */
int fatfs_run_all_tests(void);

/**
 * @brief  FATFS 简单读写自测：挂载 1:、写 fatfs_test.txt 再读回校验。
 * @note   默认无 FAT 时不自动 f_mkfs（避免破坏 PC 分区）；见 FATFS_RW_AUTO_MKFS。
 */
FRESULT fatfs_file_rw_test(void);

/**
 * @brief  读取 1:/123.txt 并打印。默认不在片上 f_mkfs（避免破坏 PC 分区）；见 FATFS_READ123_AUTO_MKFS。
 */
FRESULT fatfs_read_123_txt_test(void);

/**
 * @brief  创建 CSV 表格文件并写入数据。默认不在片上 f_mkfs（避免破坏 PC 分区）；见 FATFS_RW_AUTO_MKFS。
 */
FRESULT fatfs_create_csv_test(void);

#endif
