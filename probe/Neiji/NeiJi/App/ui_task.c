#include "ui_task.h"

#include "main.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lcd_rgb.h"
#include "ltdc.h"
#include "uart_diag.h"

#include <stdio.h>
#include <stdbool.h>

#define UI_TASK_STACK_SIZE  (1024U * 12U)
#define LVGL_BG565          0x2104U

#define UI_TEST_BAR_H       24U

static void UiTask(void *argument);
static lv_obj_t *Ui_CreateBootScreen(void);

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
        char msg[56];

        (void)snprintf(msg, sizeof(msg),
                       "[ui] osThreadNew FAILED heap=%lu\r\n",
                       (unsigned long)xPortGetFreeHeapSize());
        UartDiag_Write(msg);
    } else {
        UartDiag_Write("[ui] osThreadNew OK\r\n");
    }
}

static bool Ui_IsBgPixel(uint16_t pix)
{
    return (pix == LVGL_BG565) || (pix == LCD_UI_BG);
}

static void Ui_ProbeFramebufLabel(const lv_area_t *label_area, uint32_t *ink_out,
                                  uint32_t *white_out, lv_coord_t *sample_lx,
                                  lv_coord_t *sample_ly, uint16_t *sample_pix,
                                  bool *sample_found)
{
    lv_coord_t lx;
    lv_coord_t ly;
    uint32_t ink = 0U;
    uint32_t white = 0U;
    bool found_white = false;
    bool found_ink = false;

    for (ly = label_area->y1; ly <= label_area->y2; ly++) {
        uint16_t px = (uint16_t)(LCD_PHYS_WIDTH - 1U - (uint16_t)ly);

        for (lx = label_area->x1; lx <= label_area->x2; lx++) {
            uint16_t py = (uint16_t)lx;
            uint16_t pix = LCD_ReadFramebufPixel(px, py);

            if (pix == 0xFFFFU) {
                white++;
                if (!found_white) {
                    *sample_lx = lx;
                    *sample_ly = ly;
                    *sample_pix = pix;
                    found_white = true;
                }
            }
            if (!Ui_IsBgPixel(pix)) {
                ink++;
                if (!found_white && !found_ink) {
                    *sample_lx = lx;
                    *sample_ly = ly;
                    *sample_pix = pix;
                    found_ink = true;
                }
            }
        }
    }

    *ink_out = ink;
    *white_out = white;
    *sample_found = found_white || found_ink;
}

static void Ui_LogScreenHealth(const char *tag)
{
    char msg[128];
    uint16_t top_pix;
    uint16_t bot_pix;
    uint16_t mid_pix;

    LCD_InvalidateFramebuf();
    top_pix = LCD_ReadFramebufPixel(0U, 0U);
    bot_pix = LCD_ReadFramebufPixel(0U, LCD_PHYS_HEIGHT - 1U);
    mid_pix = LCD_ReadFramebufPixel(LCD_PHYS_WIDTH / 2U, LCD_PHYS_HEIGHT / 2U);

    (void)snprintf(msg, sizeof(msg),
                   "[fb] %s top=0x%04X mid=0x%04X bot=0x%04X\r\n",
                   tag,
                   (unsigned long)top_pix,
                   (unsigned long)mid_pix,
                   (unsigned long)bot_pix);
    UartDiag_Write(msg);
    LCD_LtdcLogState(tag);
}

static void Ui_LogFramebufProbe(const char *tag, const lv_area_t *label_area)
{
    lv_coord_t sample_lx = 0;
    lv_coord_t sample_ly = 0;
    uint32_t fb_ink;
    uint32_t fb_white;
    uint16_t sample_pix = 0U;
    char msg[128];
    bool found;

    LCD_InvalidateFramebuf();
    Ui_ProbeFramebufLabel(label_area, &fb_ink, &fb_white,
                          &sample_lx, &sample_ly, &sample_pix, &found);

    if (found) {
        (void)snprintf(msg, sizeof(msg),
                       "[fb] %s ink=%lu white=%lu sample(%d,%d)=0x%04X\r\n",
                       tag, (unsigned long)fb_ink, (unsigned long)fb_white,
                       (int)sample_lx, (int)sample_ly, (unsigned long)sample_pix);
    } else {
        (void)snprintf(msg, sizeof(msg),
                       "[fb] %s ink=%lu white=%lu sample=none\r\n",
                       tag, (unsigned long)fb_ink, (unsigned long)fb_white);
    }
    UartDiag_Write(msg);
}

static void Ui_ProbePhysRect(const char *tag, uint16_t px, uint16_t py,
                              uint16_t w, uint16_t h, uint16_t expect)
{
    uint32_t match = 0U;
    uint32_t total = (uint32_t)w * h;
    uint16_t y;
    uint16_t x;
    char msg[128];

    for (y = 0U; y < h; y++) {
        for (x = 0U; x < w; x++) {
            if (LCD_ReadFramebufPixel((uint16_t)(px + x), (uint16_t)(py + y)) == expect) {
                match++;
            }
        }
    }

    (void)snprintf(msg, sizeof(msg),
                   "[test] %s px=%u py=%u match=%lu/%lu expect=0x%04X\r\n",
                   tag, (unsigned)px, (unsigned)py,
                   (unsigned long)match, (unsigned long)total,
                   (unsigned long)expect);
    UartDiag_Write(msg);
}

static void Ui_LogLtdcProbe(void)
{
    char msg[128];
    uint32_t cfbar = LCD_LtdcLayer0CFBAR();
    uint32_t lcr = LTDC_Layer1->CR;
    uint32_t gcr = LTDC->GCR;

    (void)snprintf(msg, sizeof(msg),
                   "[ltdc] CFBAR=0x%08lX fb=0x%08lX LEN=%lu LTDCEN=%lu\r\n",
                   (unsigned long)cfbar,
                   (unsigned long)LCD_FramebufBaseAddr(),
                   (unsigned long)((lcr & LTDC_LxCR_LEN) != 0U),
                   (unsigned long)((gcr & LTDC_GCR_LTDCEN) != 0U));
    UartDiag_Write(msg);
}

static void Ui_LogLtdcOnly(const char *tag)
{
    LCD_LtdcLogState(tag);
}

static void UiTask(void *argument)
{
    lv_disp_t *disp;
    lv_timer_t *refr_timer;
    lv_obj_t *label;
    lv_area_t label_area;
    char msg[96];
    uint32_t probe_tick;

    (void)argument;

    lv_init();
    lv_port_disp_init();
    disp = lv_disp_get_default();
    if (disp != NULL) {
        lv_disp_set_theme(disp, NULL);
    }
    label = Ui_CreateBootScreen();

    lv_obj_update_layout(lv_scr_act());
    lv_obj_get_coords(label, &label_area);
    lv_port_disp_set_probe_area(&label_area);

    disp_enable_update();
    lv_refr_now(NULL);
    LCD_FlushWait();

    /* LVGL 之后用 DMA2D 直接画物理色条，绕过旋转，用于区分送显 vs 字体 */
    LCD_FillPhysRect(0U, 0U, LCD_PHYS_WIDTH, UI_TEST_BAR_H, WHITE);
    LCD_FillPhysRect(0U, LCD_PHYS_HEIGHT - UI_TEST_BAR_H, LCD_PHYS_WIDTH, UI_TEST_BAR_H, RED);
    UartDiag_Write("[test] phys bars drawn: top=white bottom=red\r\n");

    disp_disable_update();

    if (disp != NULL) {
        refr_timer = disp->refr_timer;
        if (refr_timer != NULL) {
            lv_timer_pause(refr_timer);
        }
    }

    (void)snprintf(msg, sizeof(msg), "[ui] ready diag=%u\r\n", (unsigned)NEIJI_DIAG_BUILD);
    UartDiag_Write(msg);

    (void)snprintf(msg, sizeof(msg),
                   "[ui] label logical (%d,%d)-(%d,%d)\r\n",
                   (int)label_area.x1, (int)label_area.y1,
                   (int)label_area.x2, (int)label_area.y2);
    UartDiag_Write(msg);

    (void)snprintf(msg, sizeof(msg),
                   "[ui] fb@0x%08lX\r\n",
                   (unsigned long)LCD_FramebufBaseAddr());
    UartDiag_Write(msg);

    Ui_LogLtdcProbe();
    LCD_InvalidateFramebuf();
    Ui_ProbePhysRect("top-white", 0U, 0U, LCD_PHYS_WIDTH, UI_TEST_BAR_H, WHITE);
    Ui_ProbePhysRect("bot-red", 0U, LCD_PHYS_HEIGHT - UI_TEST_BAR_H,
                     LCD_PHYS_WIDTH, UI_TEST_BAR_H, RED);
    Ui_LogScreenHealth("now");
    Ui_LogFramebufProbe("now", &label_area);

    probe_tick = 0U;
    UartDiag_Write("[ui] periodic: ltdc-only probe (no fb read)\r\n");
    for (;;) {
        (void)osDelay(1000);
        probe_tick++;
        if (probe_tick <= 5U) {
            char tag[16];

            (void)snprintf(tag, sizeof(tag), "t+%lus", (unsigned long)probe_tick);
            Ui_LogLtdcOnly(tag);
        }
    }
}

static lv_obj_t *Ui_CreateBootScreen(void)
{
    lv_obj_t *scr;
    lv_obj_t *label;

    scr = lv_scr_act();
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(scr);
    lv_label_set_text(label, "NeiJi LVGL step 1");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_center(label);

    return label;
}
