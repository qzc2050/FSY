#include "screens/ui_start.h"

#include "screens/ui_Main_Interface.h"

#include "../ui.h"

#include "lvgl.h"

#define UI_START_BG_COLOR      0x0E1117U
#define UI_START_PANEL_COLOR   0x1A2332U
#define UI_START_TEXT_COLOR    0xEAF2FFU
#define UI_START_ACCENT_COLOR  0x33C2FFU
#define UI_START_HINT_COLOR    0x9AB0CFU

#define UI_START_AUTO_ENTER_MS 1500U

static lv_obj_t *s_start_scr;
static lv_timer_t *s_enter_timer;

static lv_color_t ui_color(uint32_t hex)
{
    return lv_color_hex(hex);
}

static void ui_start_enter_main(void)
{
    if (s_enter_timer != NULL) {
        lv_timer_del(s_enter_timer);
        s_enter_timer = NULL;
    }

    ui_Main_Interface_screen_init();
}

static void start_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if ((code == LV_EVENT_CLICKED) || (code == LV_EVENT_RELEASED)) {
        ui_start_enter_main();
    }
}

static void start_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_start_enter_main();
}

void ui_start_create(void)
{
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *start_btn;
    lv_obj_t *start_btn_label;
    lv_obj_t *hint;
    lv_group_t *group;

    s_start_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_start_scr);
    lv_obj_set_style_bg_color(s_start_scr, ui_color(UI_START_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_start_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_start_scr, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(s_start_scr);
    lv_label_set_text(title, "\x46\x53\x59\x2D\x49\x20\xE6\x9C\xAC\xE6\x9C\xBA");
    lv_obj_set_style_text_font(title, &ui_font_hansanbold28, 0);
    lv_obj_set_style_text_color(title, ui_color(UI_START_TEXT_COLOR), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -120);

    subtitle = lv_label_create(s_start_scr);
    lv_label_set_text(subtitle, "NeiJi Firmware");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(subtitle, ui_color(UI_START_HINT_COLOR), 0);
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);

    start_btn = lv_btn_create(s_start_scr);
    lv_obj_set_size(start_btn, 320, 96);
    lv_obj_set_style_bg_color(start_btn, ui_color(UI_START_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(start_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(start_btn, ui_color(UI_START_ACCENT_COLOR), 0);
    lv_obj_set_style_border_width(start_btn, 2, 0);
    lv_obj_set_style_radius(start_btn, 16, 0);
    lv_obj_align(start_btn, LV_ALIGN_CENTER, 0, 56);
    lv_obj_add_event_cb(start_btn, start_btn_event_cb, LV_EVENT_ALL, NULL);

    start_btn_label = lv_label_create(start_btn);
    lv_label_set_text(start_btn_label, "\xE8\xBF\x94\xE5\x9B\x9E\xE4\xB8\xBB\xE9\xA1\xB5");
    lv_obj_set_style_text_font(start_btn_label, &ui_font_hansanbold28, 0);
    lv_obj_set_style_text_color(start_btn_label, ui_color(UI_START_TEXT_COLOR), 0);
    lv_obj_center(start_btn_label);

    hint = lv_label_create(s_start_scr);
    lv_label_set_text(hint, "\xE6\x8C\x89\x20\x45\x4E\x20\xE9\x94\xAE\xE6\x88\x96\xE7\xAD\x89\xE5\xBE\x85\xE8\x87\xAA\xE5\x8A\xA8\xE8\xBF\x9B\xE5\x85\xA5");
    lv_obj_set_style_text_font(hint, &ui_font_hansanbold28, 0);
    lv_obj_set_style_text_color(hint, ui_color(UI_START_HINT_COLOR), 0);
    lv_obj_align_to(hint, start_btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);

    group = lv_group_get_default();
    if (group != NULL) {
        lv_group_focus_obj(start_btn);
    }

    lv_scr_load(s_start_scr);

    s_enter_timer = lv_timer_create(start_timer_cb, UI_START_AUTO_ENTER_MS, NULL);
}
