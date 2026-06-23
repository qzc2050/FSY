#include "flash_fs_mutex.h"

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_flash_mutex;

void flash_fs_mutex_init(void)
{
    if (s_flash_mutex == NULL) {
        s_flash_mutex = xSemaphoreCreateMutex();
    }
}

void flash_fs_lock(void)
{
    if (s_flash_mutex != NULL) {
        (void)xSemaphoreTake(s_flash_mutex, portMAX_DELAY);
    }
}

void flash_fs_unlock(void)
{
    if (s_flash_mutex != NULL) {
        (void)xSemaphoreGive(s_flash_mutex);
    }
}
