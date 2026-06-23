#ifndef FLASH_FS_MUTEX_H
#define FLASH_FS_MUTEX_H

void flash_fs_mutex_init(void);
void flash_fs_lock(void);
void flash_fs_unlock(void);

#endif /* FLASH_FS_MUTEX_H */
