#include "ui_task.h"

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "uart_diag.h"

#define UI_TASK_STACK_SIZE  (1024U * 12U)
#define UI_LVGL_TICK_MS     5U

static void UiTask(void *argument);
static void Ui_CreateBootScreen(void);

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
    Ui_CreateBootScreen();

    UartDiag_Write("[ui] ready\r\n");

    lv_timer_handler();
    (void)osDelay(UI_LVGL_TICK_MS);

    for (;;) {
        (void)lv_timer_handler();
        (void)osDelay(UI_LVGL_TICK_MS);
    }
}

static void Ui_CreateBootScreen(void)
{
    lv_obj_t *scr;
    lv_obj_t *label;

    scr = lv_scr_act();
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(scr);
    lv_label_set_text(label, "NeiJi LVGL");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_center(label);
}
