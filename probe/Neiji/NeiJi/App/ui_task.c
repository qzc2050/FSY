#include "ui_task.h"

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui.h"
#include "uart_diag.h"

#define UI_TASK_STACK_SIZE  (1024U * 12U)
#define UI_LVGL_TICK_MS     5U

static void UiTask(void *argument);

static osThreadId_t uiTaskHandle;
static const osThreadAttr_t uiTaskAttributes = {
    .name = "uiTask",
    .stack_size = UI_TASK_STACK_SIZE,
    .priority = (osPriority_t)osPriorityAboveNormal,
};

void Ui_TaskInit(void)
{
    uiTaskHandle = osThreadNew(UiTask, NULL, &uiTaskAttributes);
    if (uiTaskHandle == NULL) {
        UartDiag_Write("[ui] osThreadNew FAILED\r\n");
    } else {
        UartDiag_Write("[ui] osThreadNew OK\r\n");
    }
}

static void UiTask(void *argument)
{
    (void)argument;

    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    ui_init();

    UartDiag_Write("[ui] ready\r\n");

    lv_timer_handler();
    (void)osDelay(UI_LVGL_TICK_MS);

    for (;;) {
        (void)lv_timer_handler();
        (void)osDelay(UI_LVGL_TICK_MS);
    }
}
