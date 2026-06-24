#include "lv_port_disp.h"

#include <stdbool.h>
#include <stdio.h>

#include "lcd_rgb.h"
#include "main.h"
#include "uart_diag.h"

#define MY_DISP_HOR_RES    LCD_WIDTH
#define MY_DISP_VER_RES    LCD_HEIGHT
#define MY_DISP_BUF_PIXELS (MY_DISP_HOR_RES * MY_DISP_VER_RES)
#define LVGL_BG565         0x2104U

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void disp_wait_cb(lv_disp_drv_t *disp_drv);
static void disp_flush_done_cb(void);

static lv_disp_drv_t *s_pending_flush_drv;
static volatile bool s_disp_flush_enabled = false;
static volatile uint32_t s_flush_cnt = 0;
static lv_area_t s_probe_area;
static volatile bool s_probe_valid = false;

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

static bool disp_find_sample_in_area(const lv_color_t *buf, const lv_area_t *a, bool want_white,
                                     lv_coord_t *sample_x, lv_coord_t *sample_y, uint16_t *sample_c)
{
    lv_coord_t x;
    lv_coord_t y;

    if ((buf == NULL) || (a == NULL)) {
        return false;
    }

    for (y = a->y1; y <= a->y2; y++) {
        for (x = a->x1; x <= a->x2; x++) {
            uint16_t c = buf[(uint32_t)y * MY_DISP_HOR_RES + (uint32_t)x].full;

            if (want_white) {
                if (c == 0xFFFFU) {
                    *sample_x = x;
                    *sample_y = y;
                    *sample_c = c;
                    return true;
                }
            } else if (c != LVGL_BG565) {
                *sample_x = x;
                *sample_y = y;
                *sample_c = c;
                return true;
            }
        }
    }

    return false;
}

static uint32_t disp_count_ink_in_area(const lv_color_t *buf, const lv_area_t *a)
{
    lv_coord_t x;
    lv_coord_t y;
    uint32_t ink = 0U;
    uint32_t white = 0U;

    if ((buf == NULL) || (a == NULL)) {
        return 0U;
    }

    for (y = a->y1; y <= a->y2; y++) {
        for (x = a->x1; x <= a->x2; x++) {
            uint16_t c = buf[(uint32_t)y * MY_DISP_HOR_RES + (uint32_t)x].full;

            if (c == 0xFFFFU) {
                white++;
            }
            if (c != LVGL_BG565) {
                ink++;
            }
        }
    }

    if (s_flush_cnt == 1U) {
        char msg[128];
        lv_coord_t sx = 0;
        lv_coord_t sy = 0;
        uint16_t sample = 0U;
        bool found;

        found = disp_find_sample_in_area(buf, a, true, &sx, &sy, &sample);
        if (!found) {
            found = disp_find_sample_in_area(buf, a, false, &sx, &sy, &sample);
        }

        if (found) {
            (void)snprintf(msg, sizeof(msg),
                           "[draw] label ink=%lu white=%lu sample(%d,%d)=0x%04X\r\n",
                           (unsigned long)ink, (unsigned long)white,
                           (int)sx, (int)sy, (unsigned long)sample);
        } else {
            (void)snprintf(msg, sizeof(msg),
                           "[draw] label ink=%lu white=%lu sample=none\r\n",
                           (unsigned long)ink, (unsigned long)white);
        }
        UartDiag_Write(msg);
    }

    return ink;
}

void lv_port_disp_set_probe_area(const lv_area_t *area)
{
    if (area != NULL) {
        s_probe_area = *area;
        s_probe_valid = true;
    } else {
        s_probe_valid = false;
    }
}

void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static __align(4) DEV_MALLOC_EXSRAM lv_color_t buf1[MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES];
    static lv_disp_drv_t disp_drv;

    s_disp_flush_enabled = false;
    s_flush_cnt = 0U;
    s_probe_valid = false;

    LCD_Dma2dInit();
    LCD_SetFlushDoneCallback(disp_flush_done_cb);

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.wait_cb = disp_wait_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;

    lv_disp_drv_register(&disp_drv);
}

void disp_enable_update(void)
{
    s_disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    s_disp_flush_enabled = false;
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (!s_disp_flush_enabled) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    {
        uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
        uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

        s_flush_cnt++;

        if (s_probe_valid) {
            (void)disp_count_ink_in_area(color_p, &s_probe_area);
        }

        if (LCD_BlitAreaAsync((uint16_t)area->x1,
                              (uint16_t)area->y1,
                              width,
                              height,
                              (const uint16_t *)color_p)) {
            s_pending_flush_drv = disp_drv;
            return;
        }
    }

    lv_disp_flush_ready(disp_drv);
}
