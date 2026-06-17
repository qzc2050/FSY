#ifndef __FLASH_FS_MUTEX_H
#define __FLASH_FS_MUTEX_H

/**
 * 替代原 FatFs 互斥：串行化 QSPI Flash 访问（与 USB MSC / 多任务协调）
 */
void flash_fs_mutex_init(void);
void flash_fs_lock(void);
void flash_fs_unlock(void);

#endif
