#include "screens/ui_main.h"

#include "../ui.h"

#include <stdio.h>

/* RAD-I 主界面配色 */
#define UI_BG_COLOR      0x121212U
#define UI_PANEL_COLOR   0x1E1E1EU
#define UI_TEXT_COLOR    0xFFFFFFU
#define UI_ACCENT_COLOR  0x00AAFFU
#define UI_MUTED_COLOR   0x888888U

lv_obj_t *ui_main_dose_label;
lv_obj_t *ui_main_status_label;

static lv_obj_t *s_scr;

static lv_color_t ui_color(uint32_t hex)
{
    return lv_color_hex(hex);
}

void ui_main_set_dose(float dose_usv_h)
{
    char buf[16];

    if (ui_main_dose_label != NULL) {
        snprintf(buf, sizeof(buf), "%.2f", (double)dose_usv_h);
        lv_label_set_text(ui_main_dose_label, buf);
    }
}

void ui_main_set_status(const char *text)
{
    if ((ui_main_status_label != NULL) && (text != NULL)) {
        lv_label_set_text(ui_main_status_label, text);
    }
}

void ui_main_create(void)
{
    lv_obj_t *title;
    lv_obj_t *dose_panel;
    lv_obj_t *unit_label;
    lv_obj_t *hint_label;

    s_scr = lv_scr_act();
    lv_obj_remove_style_all(s_scr);
    lv_obj_set_style_bg_color(s_scr, ui_color(UI_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(s_scr);
    lv_label_set_text(title, "报警显示");
    lv_obj_set_style_text_font(title, &ui_font_hansanbold32, 0);
    lv_obj_set_style_text_color(title, ui_color(UI_TEXT_COLOR), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    dose_panel = lv_obj_create(s_scr);
    lv_obj_set_size(dose_panel, 760, 220);
    lv_obj_align(dose_panel, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(dose_panel, ui_color(UI_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(dose_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dose_panel, 0, 0);
    lv_obj_set_style_radius(dose_panel, 12, 0);
    lv_obj_clear_flag(dose_panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_main_dose_label = lv_label_create(dose_panel);
    lv_label_set_text(ui_main_dose_label, "0.00");
    lv_obj_set_style_text_font(ui_main_dose_label, &ui_font_hansanbold64, 0);
    lv_obj_set_style_text_color(ui_main_dose_label, ui_color(UI_ACCENT_COLOR), 0);
    lv_obj_align(ui_main_dose_label, LV_ALIGN_CENTER, -40, -10);

    unit_label = lv_label_create(dose_panel);
    lv_label_set_text(unit_label, "μSv/h");
    lv_obj_set_style_text_font(unit_label, &ui_font_hansanbold64, 0);
    lv_obj_set_style_text_color(unit_label, ui_color(UI_MUTED_COLOR), 0);
    lv_obj_align(unit_label, LV_ALIGN_CENTER, 180, 20);

    hint_label = lv_label_create(s_scr);
    lv_label_set_text(hint_label, "系统语言");
    lv_obj_set_style_text_font(hint_label, &ui_font_hansanbold32, 0);
    lv_obj_set_style_text_color(hint_label, ui_color(UI_MUTED_COLOR), 0);
    lv_obj_align(hint_label, LV_ALIGN_CENTER, 0, 130);

    ui_main_status_label = lv_label_create(s_scr);
    lv_label_set_text(ui_main_status_label, "IP: --");
    lv_obj_set_style_text_font(ui_main_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ui_main_status_label, ui_color(UI_MUTED_COLOR), 0);
    lv_obj_align(ui_main_status_label, LV_ALIGN_BOTTOM_MID, 0, -28);

    ui_MainMenu_create(s_scr);
}
