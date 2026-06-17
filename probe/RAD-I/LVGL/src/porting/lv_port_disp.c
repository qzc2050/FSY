/**
 * @file lv_port_disp_templ.c
 *
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>

#include "lcd_rgb.h"
#include "main.h"

#if LV_USE_GPU_STM32_DMA2D
#include "../draw/stm32_dma2d/lv_gpu_stm32_dma2d.h"
#endif

#define MY_DISP_HOR_RES    LCD_WIDTH
#define MY_DISP_VER_RES    LCD_HEIGHT

#ifndef MY_DISP_HOR_RES
    #define MY_DISP_HOR_RES    854
#endif

#ifndef MY_DISP_VER_RES
    #define MY_DISP_VER_RES    480
#endif

/*
 * 未启用 LVGL sw_rotate：
 * UI 逻辑 854×480，物理显存 480×854，全宽 flush 时 area_w(854) > ver_res(480)，
 * LVGL 内置 sw_rotate 坐标会溢出。旋转在 lcd_rgb.c 的 dirty 区域转置中完成。
 */

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
static void disp_wait_cb(lv_disp_drv_t * disp_drv);
static void disp_flush_done_cb(void);

static lv_disp_drv_t * s_pending_flush_drv;

/**********************
 *  STATIC FUNCTIONS
 **********************/
static void disp_flush_done_cb(void)
{
    if(s_pending_flush_drv != NULL) {
        lv_disp_drv_t * drv = s_pending_flush_drv;

        s_pending_flush_drv = NULL;
        lv_disp_flush_ready(drv);
    }
}

static void disp_wait_cb(lv_disp_drv_t * disp_drv)
{
    (void)disp_drv;
    LCD_FlushWait();
}

void lv_port_disp_init(void)
{
    disp_init();
    LCD_Dma2dInit();
    LCD_SetFlushDoneCallback(disp_flush_done_cb);

#if LV_USE_GPU_STM32_DMA2D
    lv_draw_stm32_dma2d_init();
#endif

    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static __align(4) DEV_MALLOC_EXSRAM lv_color_t buf_2_1[MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES];
    static __align(4) DEV_MALLOC_EXSRAM lv_color_t buf_2_2[MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES];
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * LCD_LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.wait_cb = disp_wait_cb;
    disp_drv.draw_buf = &draw_buf_dsc_2;

    lv_disp_drv_register(&disp_drv);
}

static void disp_init(void)
{
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

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(disp_flush_enabled) {
        uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
        uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);

        if(LCD_BlitAreaAsync((uint16_t)area->x1,
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

#else
typedef int keep_pedantic_happy;
#endif
