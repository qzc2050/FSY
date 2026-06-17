#include "fatfs_test.h"
#include "exfuns.h"
#include "usart.h"
#include "ff.h"
#include "string.h"
#include "FreeRTOS.h"
#include "semphr.h"
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

static uint8_t s_fatfs_exfuns_inited;
/** 与 f_mount(fs[1],"1:",1) / f_mount(0,"1:",0) 成对维护，供业务与 USB MSC 切换 */
static uint8_t s_fatfs_vol1_mounted;

static SemaphoreHandle_t s_fatfs_fs_mutex;

void fatfs_fs_mutex_init(void)
{
	if (s_fatfs_fs_mutex == NULL) {
		s_fatfs_fs_mutex = xSemaphoreCreateMutex();
	}
}

void fatfs_fs_lock(void)
{
	if (s_fatfs_fs_mutex != NULL) {
		(void)xSemaphoreTake(s_fatfs_fs_mutex, portMAX_DELAY);
	}
}

void fatfs_fs_unlock(void)
{
	if (s_fatfs_fs_mutex != NULL) {
		(void)xSemaphoreGive(s_fatfs_fs_mutex);
	}
}

/**
 * @brief  为 fs[]/file 等分配内存（必须在使用 f_mount(fs[1],...) 之前调用一次）。
 *         关闭 RUN_FATFS_TESTS 时原先不会跑测试，导致从未 exfuns_init → err=12 FR_NOT_ENABLED。
 */
FRESULT fatfs_ensure_exfuns(void)
{
	if (s_fatfs_exfuns_inited) {
		return FR_OK;
	}
	if (exfuns_init() != 0) {
		printf("[FATFS] exfuns_init failed\r\n");
		return FR_NOT_ENOUGH_CORE;
	}
	s_fatfs_exfuns_inited = 1;
	return FR_OK;
}

FRESULT fatfs_vol1_ensure_mounted(void)
{
	FRESULT res;
	DWORD nclst;
	FATFS *fsap;

	res = fatfs_ensure_exfuns();
	if (res != FR_OK) {
		return res;
	}
	if (fs[1] == NULL) {
		return FR_NOT_ENABLED;
	}
	/* 标志为已挂载时仍用 f_getfree 校验：测试代码可能直接 f_mount(0) 导致不同步 */
	if (s_fatfs_vol1_mounted) {
		res = f_getfree((const TCHAR *)"1:", &nclst, &fsap);
		if (res == FR_OK) {
			return FR_OK;
		}
		s_fatfs_vol1_mounted = 0U;
	}
	res = f_mount(fs[1], (const TCHAR *)"1:", 1);
	if (res == FR_OK) {
		s_fatfs_vol1_mounted = 1U;
	}
	return res;
}

FRESULT fatfs_vol1_unmount(void)
{
	FRESULT res;

	res = f_mount(0, (const TCHAR *)"1:", 0);
	if (res == FR_OK) {
		s_fatfs_vol1_mounted = 0U;
	}
	return res;
}

int fatfs_vol1_is_mounted(void)
{
	return (s_fatfs_vol1_mounted != 0U) ? 1 : 0;
}

FRESULT fatfs_file_rw_test(void)
{
	FRESULT res;
	const TCHAR vol[] = "1:";
	const TCHAR fpath[] = "1:/TST.TXT";
	const char wdata[] = "FATFS R/W OK\r\n";
	uint8_t rbuf[48];
	UINT n;

	res = fatfs_ensure_exfuns();
	if (res != FR_OK) {
		return res;
	}

	res = f_mount(fs[1], vol, 1);
	if (res == FR_NO_FILESYSTEM) {
#if FATFS_RW_AUTO_MKFS
		printf("[FATFS] RW 测试：无 FAT，片上 f_mkfs（将破坏 PC 分区，仅离线调试用）\r\n");
		res = f_mkfs(vol, FM_ANY, 0, fatbuf, FF_MAX_SS);
		if (res != FR_OK) {
			printf("[FATFS] f_mkfs err=%d\r\n", (int)res);
			return res;
		}
		res = f_mount(fs[1], vol, 1);
#else
		printf("[FATFS] RW 测试：err13 未执行 f_mkfs（避免破坏电脑已格式化的卷）。\r\n");
		printf("[FATFS] 请用 PC 先 FAT32 格式化 U 盘，或设 FATFS_RW_AUTO_MKFS=1 后仅离线做 mkfs。\r\n");
		return res;
#endif
	}
	if (res != FR_OK) {
		printf("[FATFS] f_mount err=%d\r\n", (int)res);
		return res;
	}

	res = f_open(file, fpath, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK) {
		printf("[FATFS] f_open(write) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}
	res = f_write(file, wdata, (UINT)strlen(wdata), &n);
	if (res != FR_OK || n != strlen(wdata)) {
		printf("[FATFS] f_write err=%d n=%u\r\n", (int)res, (unsigned)n);
		f_close(file);
		f_mount(0, vol, 0);
		return (res != FR_OK) ? res : FR_INT_ERR;
	}
	res = f_sync(file);
	if (res != FR_OK) {
		printf("[FATFS] f_sync err=%d\r\n", (int)res);
		f_close(file);
		f_mount(0, vol, 0);
		return res;
	}
	res = f_close(file);
	if (res != FR_OK) {
		printf("[FATFS] f_close(after write) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}
	printf("[FATFS] wrote %u bytes\r\n", (unsigned)n);

	res = f_open(file, fpath, FA_READ);
	if (res != FR_OK) {
		printf("[FATFS] f_open(read) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}
	memset(rbuf, 0, sizeof(rbuf));
	res = f_read(file, rbuf, sizeof(rbuf) - 1U, &n);
	if (res != FR_OK) {
		printf("[FATFS] f_read err=%d\r\n", (int)res);
		f_close(file);
		f_mount(0, vol, 0);
		return res;
	}
	res = f_close(file);
	if (res != FR_OK) {
		printf("[FATFS] f_close(after read) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}
	if (n != strlen(wdata) || strcmp((char *)rbuf, wdata) != 0) {
		printf("[FATFS] verify FAIL (n=%u)\r\n", (unsigned)n);
		f_mount(0, vol, 0);
		return FR_INT_ERR;
	}
	printf("[FATFS] read back OK: %s", rbuf);

	f_mount(0, vol, 0);
	return FR_OK;
}

FRESULT fatfs_read_123_txt_test(void)
{
	FRESULT res;
	const TCHAR vol[] = "1:";
	const TCHAR fpath[] = "1:/123.txt";
	UINT n;

	res = fatfs_ensure_exfuns();
	if (res != FR_OK) {
		return res;
	}

	res = f_mount(fs[1], vol, 1);
	if (res == FR_NO_FILESYSTEM) {
#if FATFS_READ123_AUTO_MKFS
		printf("[FATFS] err13：片上 f_mkfs（将覆盖与 PC 共用的 Flash 分区头）\r\n");
		res = f_mkfs(vol, FM_ANY, 0, fatbuf, FF_MAX_SS);
		if (res != FR_OK) {
			printf("[FATFS] f_mkfs err=%d\r\n", (int)res);
			return res;
		}
		res = f_mount(fs[1], vol, 1);
#else
		printf("[FATFS] err13：无 FAT。请用电脑对 U 盘格式化为 FAT32，或先运行 fatfs_file_rw_test；\r\n");
		printf("[FATFS] 勿在 PC 已能读盘后执行片上 f_mkfs，否则会破坏 MBR。需片上 mkfs 时设 FATFS_READ123_AUTO_MKFS=1 并全工程编译。\r\n");
		return res;
#endif
	}
	if (res != FR_OK) {
		printf("[FATFS] f_mount err=%d\r\n", (int)res);
		return res;
	}

	res = f_open(file, fpath, FA_READ);
	if (res != FR_OK) {
		if (res == FR_NO_FILE) {
			printf("[FATFS] 123.txt 不存在，跳过该项测试。\r\n");
		} else {
			printf("[FATFS] open 123.txt err=%d\r\n", (int)res);
		}
		f_mount(0, vol, 0);
		return (res == FR_NO_FILE) ? FR_OK : res;
	}

	printf("[FATFS] ----- 123.txt -----\r\n");
	for (;;) {
		res = f_read(file, fatbuf, 512, &n);
		if (res != FR_OK) {
			printf("\r\n[FATFS] f_read err=%d\r\n", (int)res);
			f_close(file);
			f_mount(0, vol, 0);
			return res;
		}
		if (n == 0) {
			break;
		}
		printf("%.*s", (int)n, (char *)fatbuf);
	}
	printf("\r\n[FATFS] ----- end -----\r\n");

	f_close(file);
	f_mount(0, vol, 0);
	return FR_OK;
}

FRESULT fatfs_create_csv_test(void)
{
	FRESULT res;
	const TCHAR vol[] = "1:";
	const TCHAR fpath[] = "1:/DATA.CSV";
	UINT n;
	const char csv_header[] = "Sensor,Value,Unit,Timestamp\r\n";
	const char csv_data[] = 
		"Temperature,25.5,Celsius,2026-03-25 10:00:00\r\n"
		"Humidity,60.2,Percent,2026-03-25 10:00:00\r\n"
		"Pressure,1013.25,hPa,2026-03-25 10:00:00\r\n"
		"CO2,450,ppm,2026-03-25 10:00:00\r\n"
		"Light,800,Lux,2026-03-25 10:00:00\r\n";

	res = fatfs_ensure_exfuns();
	if (res != FR_OK) {
		return res;
	}

	res = f_mount(fs[1], vol, 1);
	if (res == FR_NO_FILESYSTEM) {
#if FATFS_RW_AUTO_MKFS
		printf("[FATFS] CSV 测试：无 FAT，片上 f_mkfs\r\n");
		res = f_mkfs(vol, FM_ANY, 0, fatbuf, FF_MAX_SS);
		if (res != FR_OK) {
			printf("[FATFS] f_mkfs err=%d\r\n", (int)res);
			return res;
		}
		res = f_mount(fs[1], vol, 1);
#else
		printf("[FATFS] CSV 测试：err13 未执行 f_mkfs。\r\n");
		printf("[FATFS] 请先用 PC 将 U 盘格式化为 FAT32。\r\n");
		return res;
#endif
	}
	if (res != FR_OK) {
		printf("[FATFS] f_mount err=%d\r\n", (int)res);
		return res;
	}

	printf("[FATFS] Creating CSV file: %s\r\n", fpath);

	res = f_open(file, fpath, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK) {
		printf("[FATFS] f_open(write) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}

	res = f_write(file, csv_header, (UINT)strlen(csv_header), &n);
	if (res != FR_OK || n != strlen(csv_header)) {
		printf("[FATFS] f_write(header) err=%d n=%u\r\n", (int)res, (unsigned)n);
		f_close(file);
		f_mount(0, vol, 0);
		return (res != FR_OK) ? res : FR_INT_ERR;
	}
	printf("[FATFS] Wrote header: %u bytes\r\n", (unsigned)n);

	res = f_write(file, csv_data, (UINT)strlen(csv_data), &n);
	if (res != FR_OK || n != strlen(csv_data)) {
		printf("[FATFS] f_write(data) err=%d n=%u\r\n", (int)res, (unsigned)n);
		f_close(file);
		f_mount(0, vol, 0);
		return (res != FR_OK) ? res : FR_INT_ERR;
	}
	printf("[FATFS] Wrote data: %u bytes\r\n", (unsigned)n);

	res = f_sync(file);
	if (res != FR_OK) {
		printf("[FATFS] f_sync err=%d\r\n", (int)res);
		f_close(file);
		f_mount(0, vol, 0);
		return res;
	}

	res = f_close(file);
	if (res != FR_OK) {
		printf("[FATFS] f_close(after write) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}

	printf("[FATFS] CSV file created successfully!\r\n");
	printf("[FATFS] Total written: %u bytes\r\n", (unsigned)(strlen(csv_header) + strlen(csv_data)));

	res = f_open(file, fpath, FA_READ);
	if (res != FR_OK) {
		printf("[FATFS] f_open(read) err=%d\r\n", (int)res);
		f_mount(0, vol, 0);
		return res;
	}

	printf("\r\n[FATFS] ----- CSV File Content -----\r\n");
	for (;;) {
		res = f_read(file, fatbuf, 512, &n);
		if (res != FR_OK) {
			printf("\r\n[FATFS] f_read err=%d\r\n", (int)res);
			f_close(file);
			f_mount(0, vol, 0);
			return res;
		}
		if (n == 0) {
			break;
		}
		printf("%.*s", (int)n, (char *)fatbuf);
	}
	printf("[FATFS] ----- End of CSV File -----\r\n");

	f_close(file);
	f_mount(0, vol, 0);
	return FR_OK;
}

int fatfs_run_all_tests(void)
{
	int tests_run = 0;
	int tests_passed = 0;
	FRESULT res;

	printf("\r\n");
	printf("========================================\r\n");
	printf("   FATFS Automated Test Suite\r\n");
	printf("========================================\r\n");
	printf("\r\n");

#if RUN_FATFS_RW_TEST
	tests_run++;
	printf("[TEST %d] Running fatfs_file_rw_test...\r\n", tests_run);
	res = fatfs_file_rw_test();
	if (res == FR_OK) {
		printf("[PASS] fatfs_file_rw_test passed\r\n");
		tests_passed++;
	} else {
		printf("[FAIL] fatfs_file_rw_test failed (err=%d)\r\n", (int)res);
	}
	printf("\r\n");
#endif

#if RUN_FATFS_READ_123_TEST
	tests_run++;
	printf("[TEST %d] Running fatfs_read_123_txt_test...\r\n", tests_run);
	res = fatfs_read_123_txt_test();
	if (res == FR_OK) {
		printf("[PASS] fatfs_read_123_txt_test passed\r\n");
		tests_passed++;
	} else {
		printf("[FAIL] fatfs_read_123_txt_test failed (err=%d)\r\n", (int)res);
	}
	printf("\r\n");
#endif

#if RUN_FATFS_CREATE_CSV_TEST
	tests_run++;
	printf("[TEST %d] Running fatfs_create_csv_test...\r\n", tests_run);
	res = fatfs_create_csv_test();
	if (res == FR_OK) {
		printf("[PASS] fatfs_create_csv_test passed\r\n");
		tests_passed++;
	} else {
		printf("[FAIL] fatfs_create_csv_test failed (err=%d)\r\n", (int)res);
	}
	printf("\r\n");
#endif

	printf("========================================\r\n");
	printf("   Test Summary\r\n");
	printf("========================================\r\n");
	printf("   Tests Run:    %d\r\n", tests_run);
	printf("   Tests Passed: %d\r\n", tests_passed);
	printf("   Tests Failed: %d\r\n", tests_run - tests_passed);
	printf("========================================\r\n");
	printf("\r\n");

	/* 各测试末尾常 f_mount(0) 卸载，恢复应用可用的 1: 挂载 */
	(void)fatfs_vol1_ensure_mounted();

	return tests_run;
}
