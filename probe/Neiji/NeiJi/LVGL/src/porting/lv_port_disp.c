#include "lv_port_disp.h"

#include <stdbool.h>

#include "lcd_rgb.h"
#include "main.h"

#define MY_DISP_HOR_RES    LCD_WIDTH
#define MY_DISP_VER_RES    LCD_HEIGHT

/*
 * 未启用 LVGL sw_rotate：
 * UI 逻辑 854×480，物理显存 480×854，旋转在 lcd_rgb.c 的 dirty 区域转置中完成。
 */

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void disp_wait_cb(lv_disp_drv_t *disp_drv);
static void disp_flush_done_cb(void);

static lv_disp_drv_t *s_pending_flush_drv;

static void disp_flush_done_cb(void)
{
    if (s_pending_flush_drv != NULL) {
        lv_disp_drv_t *drv = s_pending_flush_drv;

        s_pending_flush_drv = NULL;
        lv_disp_flush_ready(drv);
    }
}

static void disp_wait_cb(lv_disp_drv_t *disp_drv)
{
    (void)disp_drv;
    LCD_FlushWait();
}

void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static __align(4) DEV_MALLOC_EXSRAM lv_color_t buf_1[MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES];
    static __align(4) DEV_MALLOC_EXSRAM lv_color_t buf_2[MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES];
    static lv_disp_drv_t disp_drv;

    LCD_Dma2dInit();
    LCD_SetFlushDoneCallback(disp_flush_done_cb);

    lv_disp_draw_buf_init(&draw_buf, buf_1, buf_2, MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.wait_cb = disp_wait_cb;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);
}

volatile bool disp_flush_enabled = true;

void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (disp_flush_enabled) {
        uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
        uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

        /* 须在启动 DMA2D 前登记，避免 TC 中断早于赋值导致 flushing 永真 */
        s_pending_flush_drv = disp_drv;
        if (LCD_BlitAreaAsync((uint16_t)area->x1,
                              (uint16_t)area->y1,
                              width,
                              height,
                              (const uint16_t *)color_p)) {
            return;
        }
        s_pending_flush_drv = NULL;
    }

    lv_disp_flush_ready(disp_drv);
}
