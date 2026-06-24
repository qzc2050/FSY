#include "key_task.h"

#include "key.h"

#include "cmsis_os.h"

#define KEY_TASK_MS  20U

static void KeyTask(void *argument);

static osThreadId_t keyTaskHandle;
static const osThreadAttr_t keyTaskAttributes = {
    .name = "keyTask",
    .stack_size = 256 * 4,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

void Key_TaskInit(void)
{
    KEY_Init();
    keyTaskHandle = osThreadNew(KeyTask, NULL, &keyTaskAttributes);
}

static void KeyTask(void *argument)
{
    (void)argument;

    for (;;) {
        KEY_Scan();
        osDelay(KEY_TASK_MS);
    }
}
