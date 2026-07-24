// This file was generated from RAD-I SquareLine layout for NeiJi static UI migration
// Business events, timers and hardware dependencies are intentionally not wired in Step3.2.

#include "ui_Main_Interface.h"

#include "../ui.h"
#include "language.h"
#include "lcd_backlight.h"
#include "lv_port_indev.h"
#include "device_config.h"
#include "fsy_regmap.h"
#include "net_config.h"
#include "geiger.h"
#include "beep.h"
#include "sensor_task.h"
#include "main.h"
#include "lcd_rgb.h"
#include "../ui_alarm_status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- guard: sidebar CLICKED bleed after open menu; skip next OK after enter subgroup ---- */
static uint8_t ui_sidebar_guard_cnt;
static bool ui_alarm_skip_next_ok;
static bool ui_display_skip_next_ok;
static bool ui_language_skip_next_ok;

static void neiji_modify_bright(float bright)
{
    LcdBacklight_SetPercent(bright);
}

static void neiji_style_sensor_panel(lv_obj_t *panel)
{
    /* 不透明预混 ≈ 原 bg 白 + opa60 @ #202020；保留毛玻璃观感，避免半透明局部 flush 竖条 */
    lv_obj_set_style_bg_color(panel, lv_color_hex(LCD_UI_PANEL_BLEND888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void neiji_roller_set_digit(lv_obj_t *roller, uint8_t digit)
{
    if (digit > 9U) {
        digit = 9U;
    }
    lv_roller_set_selected(roller, 9 - (int)digit, LV_ANIM_OFF);
}

/* lv_group_focus_obj() 会强制 editing=false，切换数位后须重新进入编辑模式 */
static void neiji_roller_focus_edit(lv_obj_t *roller)
{
    lv_group_t *g;

    if (roller == NULL) {
        return;
    }
    g = lv_obj_get_group(roller);
    if (g == NULL) {
        return;
    }
    lv_group_focus_obj(roller);
    lv_group_set_editing(g, true);
}

static void neiji_dose_x100_to_rollers(uint32_t dose_x100,
                                       lv_obj_t *left3, lv_obj_t *left2, lv_obj_t *left1,
                                       lv_obj_t *right1, lv_obj_t *right2, lv_obj_t *unit_roller)
{
    neiji_roller_set_digit(left3, (uint8_t)((dose_x100 / 10000U) % 10U));
    neiji_roller_set_digit(left2, (uint8_t)((dose_x100 / 1000U) % 10U));
    neiji_roller_set_digit(left1, (uint8_t)((dose_x100 / 100U) % 10U));
    neiji_roller_set_digit(right1, (uint8_t)((dose_x100 / 10U) % 10U));
    neiji_roller_set_digit(right2, (uint8_t)(dose_x100 % 10U));
    if (unit_roller != NULL) {
        lv_roller_set_selected(unit_roller, 1, LV_ANIM_OFF); /* μSv/h */
    }
}

static uint32_t neiji_rollers_to_dose_x100(lv_obj_t *left3, lv_obj_t *left2, lv_obj_t *left1,
                                             lv_obj_t *right1, lv_obj_t *right2, lv_obj_t *unit)
{
    char change_val[16] = {0};
    char l3[2], l2[2], l1[2], r1[2], r2[2], unit_str[8];
    float fval;

    lv_roller_get_selected_str(left3, l3, sizeof(l3));
    lv_roller_get_selected_str(left2, l2, sizeof(l2));
    lv_roller_get_selected_str(left1, l1, sizeof(l1));
    lv_roller_get_selected_str(right1, r1, sizeof(r1));
    lv_roller_get_selected_str(right2, r2, sizeof(r2));
    lv_roller_get_selected_str(unit, unit_str, sizeof(unit_str));

    if (l3[0] == '0') {
        if (l2[0] != '0') {
            strcat(change_val, l2);
        }
    } else {
        strcat(change_val, l3);
        strcat(change_val, l2);
    }
    strcat(change_val, l1);
    strcat(change_val, ".");
    strcat(change_val, r1);
    strcat(change_val, r2);

    fval = (float)atof(change_val);
    if (strcmp(unit_str, "mSv/h") == 0) {
        fval *= 1000.0f;
    }
    if (fval < 0.0f) {
        fval = 0.0f;
    }
    return (uint32_t)(fval * 100.0f + 0.5f);
}

static void neiji_format_dose_threshold_label(lv_obj_t *label, uint32_t dose_x100)
{
    char buf[32];

    if (label == NULL) {
        return;
    }
    snprintf(buf, sizeof(buf), "%.2f \xCE\xBCSv/h", (double)dose_x100 / 100.0);
    lv_label_set_text(label, buf);
}

static uint16_t neiji_datetime_display_year(uint16_t year)
{
    if (year >= 2000U) {
        return year;
    }
    return (uint16_t)(2000U + (year % 100U));
}

static uint8_t neiji_datetime_weekday(uint16_t year2, uint8_t month, uint8_t day)
{
    uint32_t y = 2000U + (year2 % 100U);
    uint32_t m = month;
    uint32_t d = day;
    uint32_t k;
    uint32_t j;
    int w;

    if (m < 3U) {
        m += 12U;
        y -= 1U;
    }
    k = y % 100U;
    j = y / 100U;
    w = (int)((d + (13U * (m + 1U)) / 5U + k + k / 4U + j / 4U + 5U * j) % 7);
    return (uint8_t)((w + 6) % 7); /* PCF85063: 0=Sunday */
}

static int neiji_datetime_rtc_sane(const Pcf85063_DateTime_t *dt)
{
    uint16_t y2;

    if (dt == NULL) {
        return 0;
    }
    y2 = (dt->year >= 2000U) ? (uint16_t)(dt->year - 2000U) : (uint16_t)(dt->year % 100U);
    if (y2 > 99U) {
        return 0;
    }
    if ((dt->month < 1U) || (dt->month > 12U)) {
        return 0;
    }
    if ((dt->day < 1U) || (dt->day > 31U)) {
        return 0;
    }
    if (dt->hour > 23U) {
        return 0;
    }
    if (dt->minute > 59U) {
        return 0;
    }
    if (dt->second > 59U) {
        return 0;
    }
    return 1;
}

static void neiji_datetime_rollers_from_rtc(const Pcf85063_DateTime_t *dt)
{
    uint16_t y2;

    if (dt == NULL) {
        return;
    }
    y2 = (dt->year >= 2000U) ? (uint16_t)(dt->year - 2000U) : (uint16_t)(dt->year % 100U);
    if (y2 > 99U) {
        y2 = 99U;
    }
    lv_roller_set_selected(ui_Year, (uint16_t)(99 - y2), LV_ANIM_OFF);
    if ((dt->month >= 1U) && (dt->month <= 12U)) {
        lv_roller_set_selected(ui_month, (uint16_t)(12 - dt->month), LV_ANIM_OFF);
    }
    if ((dt->day >= 1U) && (dt->day <= 31U)) {
        lv_roller_set_selected(ui_day, (uint16_t)(31 - dt->day), LV_ANIM_OFF);
    }
    if (dt->hour <= 23U) {
        lv_roller_set_selected(ui_hour, (uint16_t)(23 - dt->hour), LV_ANIM_OFF);
    }
    if (dt->minute <= 59U) {
        lv_roller_set_selected(ui_minute, (uint16_t)(59 - dt->minute), LV_ANIM_OFF);
    }
}

static int neiji_datetime_rtc_from_rollers(Pcf85063_DateTime_t *dt)
{
    if (dt == NULL) {
        return -1;
    }

    /* 用滚轮索引换算，避免 INFINITE 模式下 get_selected_str 与显示不一致 */
    dt->year = (uint16_t)(99U - (lv_roller_get_selected(ui_Year) % 100U));
    dt->month = (uint8_t)(12U - (lv_roller_get_selected(ui_month) % 12U));
    dt->day = (uint8_t)(31U - (lv_roller_get_selected(ui_day) % 31U));
    dt->hour = (uint8_t)(23U - (lv_roller_get_selected(ui_hour) % 24U));
    dt->minute = (uint8_t)(59U - (lv_roller_get_selected(ui_minute) % 60U));
    dt->second = 0U;
    dt->week = neiji_datetime_weekday(dt->year, dt->month, dt->day);
    dt->online = 1U;
    return 0;
}

/* ---- helper: restore button group focus after sub-panel exit ---- */
static void neiji_restore_alarm_group(void);
static void neiji_restore_display_group(void);
static void neiji_show_only_panel(lv_obj_t *panel);
static void neiji_adjust_about_ui_layout(void);

static void neiji_bind_about_info(void)
{
    if (!DeviceConfig_IsReady()) {
        return;
    }
    if (ui_product_name) {
        /* 中/英全称均走语言表（Flash 产品名字段仅 16 字节存简称） */
        lv_label_set_text(ui_product_name, language_get_string(LANG_VALUE_PRODUCT_NAME));
    }
    if (ui_product_model) {
        lv_label_set_text(ui_product_model, DeviceConfig_GetProductModel());
    }
    if (ui_product_SN) {
        lv_label_set_text(ui_product_SN, DeviceConfig_GetSn());
    }
    if (ui_software_version) {
        lv_label_set_text(ui_software_version, DEVICE_SOFTWARE_VERSION);
    }
    neiji_adjust_about_ui_layout();
}

static void neiji_enter_about_viewing(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    neiji_bind_about_info();
    if (ui_SysAbout != NULL) {
        lv_obj_set_scroll_dir(ui_SysAbout, LV_DIR_VER);
        lv_obj_update_layout(ui_Panel_about);
        lv_obj_scroll_to_y(ui_SysAbout, 0, LV_ANIM_OFF);
    }
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_SysAbout);
    lv_group_focus_obj(ui_SysAbout);
    lv_group_set_editing(g, true);
}

static void neiji_restore_about_sidebar(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    if (ui_SysAbout != NULL) {
        lv_obj_scroll_to_y(ui_SysAbout, 0, LV_ANIM_OFF);
    }
    neiji_show_only_panel(NULL);
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_alarm_setting);
    lv_group_add_obj(g, ui_display_setting);
    lv_group_add_obj(g, ui_datetime_setting);
    lv_group_add_obj(g, ui_about);
    lv_group_focus_obj(ui_about);
    lv_group_set_editing(g, false);
}

/* ---- 1-second refresh timer: push live data to main UI labels ---- */
static void neiji_refresh_timer_cb(lv_timer_t *tmr)
{
    char buf[24];

    (void)tmr;

    /* 勿用 lv_label_set_text_fmt + %f：lv_conf.h 中 LV_SPRINTF_USE_FLOAT=0，
     * lv_snprintf 不支持浮点，会只输出格式串里的字面量 'f'（大数字体则显示缺字方框）。 */
    if (ui_data_doserate != NULL) {
        float rate = data_var.real_rate;
        /* guard against NaN / Inf from uninitialized geiger data */
        if (!(rate >= 0.0f) || rate > 9999999.0f) rate = 0.0f;
        snprintf(buf, sizeof(buf), "%.2f", (double)rate);
        lv_label_set_text(ui_data_doserate, buf);
    }
    if (ui_data_temperature != NULL) {
        float t = env_data.temperature;
        if (!(t >= -99.0f) || t > 999.0f) t = 0.0f;
        snprintf(buf, sizeof(buf), "%.1f \xE2\x84\x83", (double)t);
        lv_label_set_text(ui_data_temperature, buf);
    }
    if (ui_data_humidity != NULL) {
        float h = env_data.humidity;
        if (!(h >= 0.0f) || h > 100.0f) h = 0.0f;
        snprintf(buf, sizeof(buf), "%.1f %%", (double)h);
        lv_label_set_text(ui_data_humidity, buf);
    }
    if (ui_data_baro != NULL) {
        float kpa = env_data.baro / 1000.0f;
        if (!(kpa >= 0.0f) || kpa > 200.0f) kpa = 0.0f;
        snprintf(buf, sizeof(buf), "%.1f kPa", (double)kpa);
        lv_label_set_text(ui_data_baro, buf);
    }
    if (ui_data_CO2 != NULL) {
        unsigned co2 = (unsigned)env_data.CO2;
        if (co2 > 9999U) co2 = 0U;
        snprintf(buf, sizeof(buf), "%uppm", co2);
        lv_label_set_text(ui_data_CO2, buf);
    }
    if (ui_data_PM2_5 != NULL) {
        unsigned pm = (unsigned)env_data.PM2_5;
        if (pm > 9999U) pm = 0U;
        snprintf(buf, sizeof(buf), "%u ug/m" "\xC2\xB3", pm);
        lv_label_set_text(ui_data_PM2_5, buf);
    }
    if (ui_date != NULL) {
        snprintf(buf, sizeof(buf), "%04u/%02u/%02u  %02u:%02u",
            (unsigned)neiji_datetime_display_year(env_data.dt.year),
            (unsigned)env_data.dt.month,
            (unsigned)env_data.dt.day,
            (unsigned)env_data.dt.hour,
            (unsigned)env_data.dt.minute);
        lv_label_set_text(ui_date, buf);
    }
}

lv_obj_t * ui_Main_Interface = NULL;
lv_obj_t * ui_Panel_temperature = NULL;
lv_obj_t * ui_temperature = NULL;
lv_obj_t * ui_data_temperature = NULL;
lv_obj_t * ui_Panel_humidity = NULL;
lv_obj_t * ui_humidity = NULL;
lv_obj_t * ui_data_humidity = NULL;
lv_obj_t * ui_Panel_baro = NULL;
lv_obj_t * ui_baro = NULL;
lv_obj_t * ui_data_baro = NULL;
lv_obj_t * ui_Panel_CO2 = NULL;
lv_obj_t * ui_CO2 = NULL;
lv_obj_t * ui_data_CO2 = NULL;
lv_obj_t * ui_Panel_PM2_5 = NULL;
lv_obj_t * ui_PM2_5 = NULL;
lv_obj_t * ui_data_PM2_5 = NULL;
lv_obj_t * ui_date = NULL;
lv_obj_t * ui_alarm_on = NULL;
lv_obj_t * ui_alarm_off = NULL;
lv_obj_t * ui_led_on = NULL;
lv_obj_t * ui_led_off = NULL;
lv_obj_t * ui_Panel_doserate = NULL;
lv_obj_t * ui_radiation = NULL;
lv_obj_t * ui_data_doserate = NULL;
lv_obj_t * ui_data_unit = NULL;
lv_obj_t * ui_MainMenu1 = NULL;
lv_obj_t * ui_sidebar = NULL;
lv_obj_t * ui_alarm_setting = NULL;
lv_obj_t * ui_display_setting = NULL;
lv_obj_t * ui_datetime_setting = NULL;
lv_obj_t * ui_about = NULL;
lv_obj_t * ui_Panel_alarm_set = NULL;
lv_obj_t * ui_Label12 = NULL;
lv_obj_t * ui_alarm_light_sw = NULL;
lv_obj_t * ui_Label11 = NULL;
lv_obj_t * ui_light_alarm_state = NULL;
lv_obj_t * ui_alarm_beep_sw = NULL;
lv_obj_t * ui_Label21 = NULL;
lv_obj_t * ui_alarm_volume = NULL;
lv_obj_t * ui_Hth_Set_Btn = NULL;
lv_obj_t * ui_Label19 = NULL;
lv_obj_t * ui_high_threshold = NULL;
lv_obj_t * ui_Lth_Set_Btn = NULL;
lv_obj_t * ui_Label22 = NULL;
lv_obj_t * ui_low_threshold = NULL;
lv_obj_t * ui_Panel_volume_set = NULL;
lv_obj_t * ui_set_alarm_volume = NULL;
lv_obj_t * ui_Label8 = NULL;
lv_obj_t * ui_Slider_volume = NULL;
lv_obj_t * ui_Panel_high_th_set = NULL;
lv_obj_t * ui_Label2 = NULL;
lv_obj_t * ui_HthLeft3 = NULL;
lv_obj_t * ui_HthLeft2 = NULL;
lv_obj_t * ui_HthLeft1 = NULL;
lv_obj_t * ui_HthRight1 = NULL;
lv_obj_t * ui_HthRight2 = NULL;
lv_obj_t * ui_HthUnit = NULL;
lv_obj_t * ui_Label1 = NULL;
lv_obj_t * ui_Panel_low_th_set = NULL;
lv_obj_t * ui_Label5 = NULL;
lv_obj_t * ui_LthLeft3 = NULL;
lv_obj_t * ui_LthLeft2 = NULL;
lv_obj_t * ui_LthLeft1 = NULL;
lv_obj_t * ui_LthRight1 = NULL;
lv_obj_t * ui_LthRight2 = NULL;
lv_obj_t * ui_LthUnit = NULL;
lv_obj_t * ui_Label3 = NULL;
lv_obj_t * ui_Panel_display_set = NULL;
lv_obj_t * ui_Label7 = NULL;
lv_obj_t * ui_language_sw = NULL;
lv_obj_t * ui_Label18 = NULL;
lv_obj_t * ui_sys_language = NULL;
lv_obj_t * ui_screen_light_sw = NULL;
lv_obj_t * ui_Label16 = NULL;
lv_obj_t * ui_sys_bright = NULL;
lv_obj_t * ui_Panel_language_set = NULL;
lv_obj_t * ui_Label17 = NULL;
lv_obj_t * ui_LanguageChineseBtn = NULL;
lv_obj_t * ui_labelChinese = NULL;
lv_obj_t * ui_LanguageEnglishBtn = NULL;
lv_obj_t * ui_labelEnglish = NULL;
lv_obj_t * ui_Panel_bright_set = NULL;
lv_obj_t * ui_set_bright = NULL;
lv_obj_t * ui_Label10 = NULL;
lv_obj_t * ui_Slider_bright = NULL;
lv_obj_t * ui_Panel_datetime_set = NULL;
lv_obj_t * ui_Label14 = NULL;
lv_obj_t * ui_Year = NULL;
lv_obj_t * ui_month = NULL;
lv_obj_t * ui_day = NULL;
lv_obj_t * ui_hour = NULL;
lv_obj_t * ui_minute = NULL;
lv_obj_t * ui_Label4 = NULL;
lv_obj_t * ui_Label13 = NULL;
lv_obj_t * ui_Label15 = NULL;
lv_obj_t * ui_Panel_about = NULL;
lv_obj_t * ui_Label20 = NULL;
lv_obj_t * ui_SysAbout = NULL;
lv_obj_t * ui_product_name = NULL;
lv_obj_t * ui_product_model = NULL;
lv_obj_t * ui_product_SN = NULL;
lv_obj_t * ui_software_version = NULL;
lv_obj_t * ui_contact_up = NULL;
lv_obj_t * ui_img_phone_number = NULL;
lv_obj_t * ui_phone_number = NULL;
lv_obj_t * ui_img_address = NULL;
lv_obj_t * ui_address = NULL;
lv_obj_t * ui_img_mailbox = NULL;
lv_obj_t * ui_mailbox = NULL;

// build funtions

// Step3.3 menu interaction (UI only, no business logic)
static void neiji_show_only_panel(lv_obj_t *panel)
{
    _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Panel_display_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Panel_datetime_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Panel_about, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    if (panel != NULL) {
        _ui_flag_modify(panel, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
}

static void neiji_open_menu(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    _ui_flag_modify(ui_MainMenu1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);

    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_alarm_setting);
    lv_group_add_obj(g, ui_display_setting);
    lv_group_add_obj(g, ui_datetime_setting);
    lv_group_add_obj(g, ui_about);
    lv_group_focus_obj(ui_alarm_setting);
    lv_group_set_editing(g, false);
    ui_sidebar_guard_cnt = 1;  /* 下一个 CLICKED 是进菜单的 OK 键余震，拦截 */
}

static void neiji_close_menu(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    neiji_show_only_panel(NULL);
    _ui_flag_modify(ui_MainMenu1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);

    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_Main_Interface);
    lv_group_focus_obj(ui_Main_Interface);
}

static void neiji_event_main(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        neiji_open_menu();
    }
}

static void neiji_enter_alarm_button_group(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_alarm_light_sw);
    lv_group_add_obj(g, ui_alarm_beep_sw);
    lv_group_add_obj(g, ui_Hth_Set_Btn);
    lv_group_add_obj(g, ui_Lth_Set_Btn);
    lv_group_focus_obj(ui_alarm_light_sw);
    lv_group_set_editing(g, false);
    ui_alarm_skip_next_ok = true;
}

static void neiji_enter_display_button_group(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_language_sw);
    lv_group_add_obj(g, ui_screen_light_sw);
    lv_group_focus_obj(ui_language_sw);
    lv_group_set_editing(g, false);
    ui_display_skip_next_ok = true;
}

static void neiji_datetime_refresh_rollers(void)
{
    Pcf85063_DateTime_t dt = env_data.dt;

    if ((Pcf85063_GetTime(&dt) != 0) || !neiji_datetime_rtc_sane(&dt)) {
        dt = env_data.dt;
    }
    if (!neiji_datetime_rtc_sane(&dt)) {
        return;
    }
    neiji_datetime_rollers_from_rtc(&dt);
}

static void neiji_enter_datetime_editing(void)
{
    lv_group_t *g = lv_port_indev_get_group();

    neiji_datetime_refresh_rollers();

    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_Year);
    lv_group_add_obj(g, ui_month);
    lv_group_add_obj(g, ui_day);
    lv_group_add_obj(g, ui_hour);
    lv_group_add_obj(g, ui_minute);
    neiji_roller_focus_edit(ui_Year);
}

/* ---- Sidebar: alarm setting ---- */
static void neiji_event_alarm_setting(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        ui_main_bind_settings();
        neiji_show_only_panel(ui_Panel_alarm_set);
    } else if (code == LV_EVENT_CLICKED) {
        /* 仅吞掉「开菜单」同一颗 OK 触发的 CLICKED 余震；真正进入用 KEY ENTER */
        if (ui_sidebar_guard_cnt) {
            ui_sidebar_guard_cnt = 0;
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        neiji_enter_alarm_button_group();
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_close_menu();
    }
}

/* ---- Sidebar: display setting ---- */
static void neiji_event_display_setting(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        neiji_show_only_panel(ui_Panel_display_set);
    } else if (code == LV_EVENT_CLICKED) {
        if (ui_sidebar_guard_cnt) {
            ui_sidebar_guard_cnt = 0;
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        neiji_enter_display_button_group();
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_close_menu();
    }
}

/* ---- Sidebar: datetime setting ---- */
static void neiji_event_datetime_setting(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        neiji_show_only_panel(ui_Panel_datetime_set);
        /* 一切到时间设置就刷新 roller，与 RAD-I 一致；勿等到按 OK 才预填 */
        neiji_datetime_refresh_rollers();
    } else if (code == LV_EVENT_CLICKED) {
        if (ui_sidebar_guard_cnt) {
            ui_sidebar_guard_cnt = 0;
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        neiji_enter_datetime_editing();
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_close_menu();
    }
}

/* ---- Sidebar: about ---- */
static void neiji_event_about(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        neiji_show_only_panel(ui_Panel_about);
        neiji_bind_about_info();
    } else if (code == LV_EVENT_CLICKED) {
        if (ui_sidebar_guard_cnt) {
            ui_sidebar_guard_cnt = 0;
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        neiji_enter_about_viewing();
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_close_menu();
    }
}

/* ===================================================================
 * ALARM BUTTON GROUP handlers
 * =================================================================== */

static void neiji_restore_alarm_group(void)
{
    lv_group_t *g = lv_port_indev_get_group();
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_alarm_light_sw);
    lv_group_add_obj(g, ui_alarm_beep_sw);
    lv_group_add_obj(g, ui_Hth_Set_Btn);
    lv_group_add_obj(g, ui_Lth_Set_Btn);
    lv_group_set_editing(g, false);  /* UP/DOWN navigates buttons */
}

/* Alarm light switch: OK toggles on/off, ESC back to sidebar */
static void neiji_event_alarm_light_sw(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        /* 进入报警项组后同一颗 OK 会余震到此，只聚焦不切换 */
        if (ui_alarm_skip_next_ok) {
            ui_alarm_skip_next_ok = false;
            return;
        }
        /* Toggle light alarm enable */
        {
            uint8_t on = (uint8_t)(sys_cfg.alarm_light ? 0U : 1U);
            (void)DeviceConfig_SetAlarmLight(on);
            if (ui_light_alarm_state) {
                lv_label_set_text(ui_light_alarm_state,
                    on ? language_get_string(LANG_STATE_ON)
                       : language_get_string(LANG_STATE_OFF));
            }
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (ui_alarm_skip_next_ok) {
            ui_alarm_skip_next_ok = false;
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        /* ESC back to sidebar menu */
        neiji_show_only_panel(NULL);
        {
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_alarm_setting);
            lv_group_add_obj(g, ui_display_setting);
            lv_group_add_obj(g, ui_datetime_setting);
            lv_group_add_obj(g, ui_about);
            lv_group_focus_obj(ui_alarm_setting);
        }
        ui_alarm_skip_next_ok = false;
    }
}

/* Beeper row: OK opens volume panel (volume saved to Flash on ESC) */
static void neiji_event_alarm_beep_sw(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        uint8_t vol;
        int32_t v;

        DeviceConfig_GetDoseAlarmConfig(NULL, NULL, NULL, &vol);
        v = (int32_t)vol;
        if (v < 0) {
            v = 0;
        } else if (v > 100) {
            v = 100;
        }
        lv_slider_set_value(ui_Slider_volume, v, LV_ANIM_OFF);
        lv_label_set_text_fmt(ui_set_alarm_volume, "%" LV_PRId32 " %%", v);
        _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_Panel_volume_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        {
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_Slider_volume);
            lv_group_focus_obj(ui_Slider_volume);
            lv_group_set_editing(g, true);
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_show_only_panel(NULL);
        {
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_alarm_setting);
            lv_group_add_obj(g, ui_display_setting);
            lv_group_add_obj(g, ui_datetime_setting);
            lv_group_add_obj(g, ui_about);
            lv_group_focus_obj(ui_alarm_setting);
        }
    }
}

/* High threshold entry: OK opens roller panel */
static void neiji_event_Hth_Set_Btn(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_Panel_high_th_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        {
            uint32_t hi;
            DeviceConfig_GetDoseAlarmConfig(&hi, NULL, NULL, NULL);
            neiji_dose_x100_to_rollers(hi, ui_HthLeft3, ui_HthLeft2, ui_HthLeft1,
                                       ui_HthRight1, ui_HthRight2, ui_HthUnit);
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_HthLeft3);
            lv_group_add_obj(g, ui_HthLeft2);
            lv_group_add_obj(g, ui_HthLeft1);
            lv_group_add_obj(g, ui_HthRight1);
            lv_group_add_obj(g, ui_HthRight2);
            lv_group_add_obj(g, ui_HthUnit);
            neiji_roller_focus_edit(ui_HthLeft3);
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_show_only_panel(NULL);
        {
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_alarm_setting);
            lv_group_add_obj(g, ui_display_setting);
            lv_group_add_obj(g, ui_datetime_setting);
            lv_group_add_obj(g, ui_about);
            lv_group_focus_obj(ui_alarm_setting);
        }
    }
}

/* Low threshold entry: OK opens roller panel */
static void neiji_event_Lth_Set_Btn(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_Panel_low_th_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        {
            uint32_t lo;
            DeviceConfig_GetDoseAlarmConfig(NULL, &lo, NULL, NULL);
            neiji_dose_x100_to_rollers(lo, ui_LthLeft3, ui_LthLeft2, ui_LthLeft1,
                                       ui_LthRight1, ui_LthRight2, ui_LthUnit);
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_LthLeft3);
            lv_group_add_obj(g, ui_LthLeft2);
            lv_group_add_obj(g, ui_LthLeft1);
            lv_group_add_obj(g, ui_LthRight1);
            lv_group_add_obj(g, ui_LthRight2);
            lv_group_add_obj(g, ui_LthUnit);
            neiji_roller_focus_edit(ui_LthLeft3);
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_show_only_panel(NULL);
        {
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_alarm_setting);
            lv_group_add_obj(g, ui_display_setting);
            lv_group_add_obj(g, ui_datetime_setting);
            lv_group_add_obj(g, ui_about);
            lv_group_focus_obj(ui_alarm_setting);
        }
    }
}

/* ===================================================================
 * DISPLAY BUTTON GROUP handlers
 * =================================================================== */

static void neiji_restore_display_group(void)
{
    lv_group_t *g = lv_port_indev_get_group();
    lv_group_remove_all_objs(g);
    lv_group_add_obj(g, ui_language_sw);
    lv_group_add_obj(g, ui_screen_light_sw);
    lv_group_set_editing(g, false);  /* UP/DOWN navigates buttons */
}

/* Screen brightness entry: OK opens slider panel */
static void neiji_event_screen_light_sw(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        _ui_flag_modify(ui_Panel_display_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_Panel_bright_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        {
            int32_t bright = (int32_t)sys_cfg.bright_sz;
            if (bright < 1) {
                bright = 1;
            } else if (bright > 100) {
                bright = 100;
            }
            lv_slider_set_value(ui_Slider_bright, bright, LV_ANIM_OFF);
            lv_label_set_text_fmt(ui_set_bright, "%" LV_PRId32 " %%", bright);
            lv_label_set_text_fmt(ui_sys_bright, "%" LV_PRId32 " %%", bright);
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_Slider_bright);
            lv_group_focus_obj(ui_Slider_bright);
            lv_group_set_editing(g, true);  /* UP/DOWN edits slider */
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_show_only_panel(NULL);
        {
            lv_group_t *g = lv_port_indev_get_group();
            lv_group_remove_all_objs(g);
            lv_group_add_obj(g, ui_alarm_setting);
            lv_group_add_obj(g, ui_display_setting);
            lv_group_add_obj(g, ui_datetime_setting);
            lv_group_add_obj(g, ui_about);
            lv_group_focus_obj(ui_display_setting);
        }
    }
}

/* ---- Language switch sub-panel ---- */
static void neiji_close_language_panel(void)
{
    _ui_flag_modify(ui_Panel_language_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Panel_display_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    neiji_restore_display_group();
    lv_group_focus_obj(ui_language_sw);
}

static void neiji_commit_language(Language_t lang)
{
    uint8_t buf[2];

    language_set_current(lang);
    sys_cfg.language = (uint8_t)lang;
    buf[0] = (uint8_t)lang;
    buf[1] = 0U;
    (void)DeviceConfig_WriteRegBlock(FSY_REG_LANGUAGE, buf, 2);
    update_all_ui_texts();
}

static void neiji_finish_language_select(Language_t lang)
{
    neiji_commit_language(lang);
    neiji_close_language_panel();
}

static Language_t neiji_language_from_focus(void)
{
    lv_obj_t *focused = lv_group_get_focused(lv_port_indev_get_group());

    if (focused == ui_LanguageEnglishBtn) {
        return LANG_ENGLISH;
    }
    return LANG_CHINESE;
}

static void neiji_confirm_language_panel(void)
{
    if (ui_language_skip_next_ok) {
        ui_language_skip_next_ok = false;
        return;
    }
    neiji_finish_language_select(neiji_language_from_focus());
}

static void neiji_enter_language_panel(void)
{
    _ui_flag_modify(ui_Panel_display_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(ui_Panel_language_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    {
        lv_group_t *g = lv_port_indev_get_group();
        lv_group_remove_all_objs(g);
        lv_group_add_obj(g, ui_LanguageChineseBtn);
        lv_group_add_obj(g, ui_LanguageEnglishBtn);
        if (language_get_current() == LANG_ENGLISH) {
            lv_group_focus_obj(ui_LanguageEnglishBtn);
        } else {
            lv_group_focus_obj(ui_LanguageChineseBtn);
        }
        lv_group_set_editing(g, false);
    }
    /* 打开面板的同一颗 OK 会在释放时落到语言按钮上，须吞掉余震 */
    ui_language_skip_next_ok = true;
}

static void neiji_event_language_sw(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ENTER) {
        if (ui_display_skip_next_ok) {
            ui_display_skip_next_ok = false;
            return;
        }
        neiji_enter_language_panel();
    } else if (code == LV_EVENT_CLICKED) {
        if (ui_display_skip_next_ok) {
            ui_display_skip_next_ok = false;
        }
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        if (lv_obj_has_flag(ui_Panel_language_set, LV_OBJ_FLAG_HIDDEN)) {
            neiji_show_only_panel(NULL);
            {
                lv_group_t *g = lv_port_indev_get_group();
                lv_group_remove_all_objs(g);
                lv_group_add_obj(g, ui_alarm_setting);
                lv_group_add_obj(g, ui_display_setting);
                lv_group_add_obj(g, ui_datetime_setting);
                lv_group_add_obj(g, ui_about);
                lv_group_focus_obj(ui_display_setting);
            }
            ui_display_skip_next_ok = false;
        } else {
            neiji_confirm_language_panel();
        }
    }
}

static void neiji_event_language_select(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key;

    /* 仅吞掉打开面板时同一颗 OK 的 CLICKED 余震 */
    if (code == LV_EVENT_CLICKED) {
        if (ui_language_skip_next_ok) {
            ui_language_skip_next_ok = false;
        }
        return;
    }

    if (code != LV_EVENT_KEY) {
        return;
    }

    key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER || key == LV_KEY_ESC) {
        neiji_confirm_language_panel();
    }
}

/* ===================================================================
 * SLIDER handlers (volume, brightness)
 * =================================================================== */

/* Volume slider: UP/DOWN edit, ESC save & return */
static void neiji_event_slider_volume(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t v = lv_slider_get_value(ui_Slider_volume);
        lv_label_set_text_fmt(ui_set_alarm_volume, "%" LV_PRId32 " %%", v);
        sys_cfg.alarm_volume = (uint8_t)v;
        Beep_Ctr(BEEP_EVENT_SETTING);
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        int32_t v = lv_slider_get_value(ui_Slider_volume);
        uint8_t buf[2];

        sys_cfg.alarm_volume = (uint8_t)v;
        if (v == 0) {
            sys_cfg.alarm_sound = 0U;
            (void)DeviceConfig_SetAlarmSound(0U);
        } else {
            sys_cfg.alarm_sound = 1U;
            (void)DeviceConfig_SetAlarmSound(1U);
        }
        Beep_Ctr(BEEP_EVENT_CLR);
        buf[0] = (uint8_t)(v & 0xFF);
        buf[1] = 0U;
        DeviceConfig_WriteRegBlock(FSY_REG_ALARM_VOLUME, buf, 2);
        if (ui_alarm_volume) {
            lv_label_set_text_fmt(ui_alarm_volume, "%" LV_PRId32 " %%", v);
        }
        _ui_flag_modify(ui_Panel_volume_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        neiji_restore_alarm_group();
        lv_group_focus_obj(ui_alarm_beep_sw);
    }
}

/* Brightness slider: UP/DOWN edit, ESC save & return */
static void neiji_event_slider_bright(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t v = lv_slider_get_value(ui_Slider_bright);
        lv_label_set_text_fmt(ui_set_bright, "%" LV_PRId32 " %%", v);
        neiji_modify_bright((float)v);
    } else if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        int32_t v = lv_slider_get_value(ui_Slider_bright);
        sys_cfg.bright_sz = (float)v;
        neiji_modify_bright((float)v);
        if (ui_sys_bright) {
            lv_label_set_text_fmt(ui_sys_bright, "%" LV_PRId32 " %%", v);
        }
        _ui_flag_modify(ui_Panel_bright_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
        _ui_flag_modify(ui_Panel_display_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
        neiji_restore_display_group();
        lv_group_focus_obj(ui_screen_light_sw);
    }
}

/* ===================================================================
 * ROLLER handlers (high/low threshold, date/time)
 * =================================================================== */

/* -- High threshold roller group -- */
static void neiji_event_roller_hth(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t k = lv_event_get_key(e);
        if (k == LV_KEY_ENTER) {
            /* OK: navigate to next roller digit */
            lv_obj_t *f = lv_group_get_focused(lv_port_indev_get_group());
            if (f == ui_HthLeft3)       neiji_roller_focus_edit(ui_HthLeft2);
            else if (f == ui_HthLeft2)   neiji_roller_focus_edit(ui_HthLeft1);
            else if (f == ui_HthLeft1)   neiji_roller_focus_edit(ui_HthRight1);
            else if (f == ui_HthRight1)  neiji_roller_focus_edit(ui_HthRight2);
            else if (f == ui_HthRight2)  neiji_roller_focus_edit(ui_HthUnit);
            else if (f == ui_HthUnit)    neiji_roller_focus_edit(ui_HthLeft3);
        } else if (k == LV_KEY_ESC) {
            uint32_t hi = neiji_rollers_to_dose_x100(ui_HthLeft3, ui_HthLeft2, ui_HthLeft1,
                                                     ui_HthRight1, ui_HthRight2, ui_HthUnit);
            {
                uint8_t buf[4];
                buf[0] = (uint8_t)(hi & 0xFF);
                buf[1] = (uint8_t)((hi >> 8) & 0xFF);
                buf[2] = (uint8_t)((hi >> 16) & 0xFF);
                buf[3] = (uint8_t)((hi >> 24) & 0xFF);
                DeviceConfig_WriteRegBlock(FSY_REG_DOSE_HI_TH, buf, 4);
            }
            {
                uint32_t saved_hi;
                DeviceConfig_GetDoseAlarmConfig(&saved_hi, NULL, NULL, NULL);
                sys_cfg.th_rh_rate = (float)saved_hi / 100.0f;
                neiji_format_dose_threshold_label(ui_high_threshold, saved_hi);
            }
            _ui_flag_modify(ui_Panel_high_th_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            neiji_restore_alarm_group();
            lv_group_focus_obj(ui_Hth_Set_Btn);
        }
    }
}

/* -- Low threshold roller group -- */
static void neiji_event_roller_lth(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t k = lv_event_get_key(e);
        if (k == LV_KEY_ENTER) {
            lv_obj_t *f = lv_group_get_focused(lv_port_indev_get_group());
            if (f == ui_LthLeft3)       neiji_roller_focus_edit(ui_LthLeft2);
            else if (f == ui_LthLeft2)   neiji_roller_focus_edit(ui_LthLeft1);
            else if (f == ui_LthLeft1)   neiji_roller_focus_edit(ui_LthRight1);
            else if (f == ui_LthRight1)  neiji_roller_focus_edit(ui_LthRight2);
            else if (f == ui_LthRight2)  neiji_roller_focus_edit(ui_LthUnit);
            else if (f == ui_LthUnit)    neiji_roller_focus_edit(ui_LthLeft3);
        } else if (k == LV_KEY_ESC) {
            uint32_t lo = neiji_rollers_to_dose_x100(ui_LthLeft3, ui_LthLeft2, ui_LthLeft1,
                                                     ui_LthRight1, ui_LthRight2, ui_LthUnit);
            {
                uint8_t buf[4];
                buf[0] = (uint8_t)(lo & 0xFF);
                buf[1] = (uint8_t)((lo >> 8) & 0xFF);
                buf[2] = (uint8_t)((lo >> 16) & 0xFF);
                buf[3] = (uint8_t)((lo >> 24) & 0xFF);
                DeviceConfig_WriteRegBlock(FSY_REG_DOSE_LO_TH, buf, 4);
            }
            {
                uint32_t saved_lo;
                DeviceConfig_GetDoseAlarmConfig(NULL, &saved_lo, NULL, NULL);
                sys_cfg.th_rl_rate = (float)saved_lo / 100.0f;
                neiji_format_dose_threshold_label(ui_low_threshold, saved_lo);
            }
            _ui_flag_modify(ui_Panel_low_th_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
            _ui_flag_modify(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
            neiji_restore_alarm_group();
            lv_group_focus_obj(ui_Lth_Set_Btn);
        }
    }
}

/* -- Date/time roller group -- */
static void neiji_event_roller_datetime(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_KEY) {
        uint32_t k = lv_event_get_key(e);
        if (k == LV_KEY_ENTER) {
            /* OK: navigate between rollers */
            lv_obj_t *f = lv_group_get_focused(lv_port_indev_get_group());
            if (f == ui_Year)         neiji_roller_focus_edit(ui_month);
            else if (f == ui_month)    neiji_roller_focus_edit(ui_day);
            else if (f == ui_day)      neiji_roller_focus_edit(ui_hour);
            else if (f == ui_hour)     neiji_roller_focus_edit(ui_minute);
            else if (f == ui_minute)   neiji_roller_focus_edit(ui_Year);
        } else if (k == LV_KEY_ESC) {
            Pcf85063_DateTime_t dt;

            if ((neiji_datetime_rtc_from_rollers(&dt) == 0) &&
                neiji_datetime_rtc_sane(&dt) &&
                (Pcf85063_SetTime(&dt) == 0)) {
                env_data.dt = dt;
            } else {
                neiji_datetime_refresh_rollers();
            }
            /* Return to sidebar */
            neiji_show_only_panel(NULL);
            {
                lv_group_t *g = lv_port_indev_get_group();
                lv_group_remove_all_objs(g);
                lv_group_add_obj(g, ui_alarm_setting);
                lv_group_add_obj(g, ui_display_setting);
                lv_group_add_obj(g, ui_datetime_setting);
                lv_group_add_obj(g, ui_about);
                lv_group_focus_obj(ui_datetime_setting);
            }
        }
    }
}

/* ===================================================================
 * ABOUT scroll handler
 * =================================================================== */

static void neiji_event_about_scroll(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    /* UP/DOWN 由 LVGL SCROLL_WITH_ARROW 处理；此处仅 ESC 返回侧边栏 */
    if (code == LV_EVENT_KEY && lv_event_get_key(e) == LV_KEY_ESC) {
        neiji_restore_about_sidebar();
    }
}

/* ===================================================================
 * ui_main_bind_settings: refresh alarm/display button labels from config
 * =================================================================== */
void ui_main_bind_settings(void)
{
    uint32_t hi, lo, mask;
    uint8_t vol, sound, light;

    if (!DeviceConfig_IsReady()) return;

    DeviceConfig_GetDoseAlarmConfig(&hi, &lo, &mask, &vol);
    DeviceConfig_GetAlarmOutput(&sound, &light, &vol);
    sys_cfg.alarm_sound = sound;
    sys_cfg.alarm_light = light;
    sys_cfg.alarm_volume = vol;
    sys_cfg.th_rh_rate = (float)hi / 100.0f;
    sys_cfg.th_rl_rate = (float)lo / 100.0f;

    if (ui_light_alarm_state) {
        lv_label_set_text(ui_light_alarm_state,
            sys_cfg.alarm_light ? language_get_string(LANG_STATE_ON)
                                : language_get_string(LANG_STATE_OFF));
    }
    if (ui_alarm_volume)
        lv_label_set_text_fmt(ui_alarm_volume, "%u %%", (unsigned)vol);
    if (ui_sys_bright)
        lv_label_set_text_fmt(ui_sys_bright, "%u %%", (unsigned)sys_cfg.bright_sz);
    if (ui_high_threshold)
        neiji_format_dose_threshold_label(ui_high_threshold, hi);
    if (ui_low_threshold)
        neiji_format_dose_threshold_label(ui_low_threshold, lo);

    neiji_modify_bright(sys_cfg.bright_sz);
    neiji_bind_about_info();
}

void ui_Main_Interface_screen_init(void)
{
    ui_Main_Interface = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Main_Interface, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_outline_color(ui_Main_Interface, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_Main_Interface, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Main_Interface, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Main_Interface, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Main_Interface, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Main_Interface, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    /* 原 SquareLine 默认 montserrat_48 仅作根屏字体且几乎被各标签覆盖，改用 LV 默认字以省 Flash */
    lv_obj_set_style_text_font(ui_Main_Interface, LV_FONT_DEFAULT, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_temperature = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_temperature, 140);
    lv_obj_set_height(ui_Panel_temperature, 155);
    lv_obj_set_x(ui_Panel_temperature, 6);
    lv_obj_set_y(ui_Panel_temperature, 0);
    lv_obj_set_align(ui_Panel_temperature, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_clear_flag(ui_Panel_temperature, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_temperature, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    neiji_style_sensor_panel(ui_Panel_temperature);
    lv_obj_set_style_border_color(ui_Panel_temperature, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel_temperature, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_temperature, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_temperature, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_temperature, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_temperature, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_temperature = lv_img_create(ui_Panel_temperature);
    lv_img_set_src(ui_temperature, &ui_img_532493655);
    lv_obj_set_width(ui_temperature, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_temperature, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_temperature, 0);
    lv_obj_set_y(ui_temperature, 5);
    lv_obj_set_align(ui_temperature, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_temperature, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_temperature, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_data_temperature = lv_label_create(ui_Panel_temperature);
    lv_obj_set_width(ui_data_temperature, 120);
    lv_obj_set_height(ui_data_temperature, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_data_temperature, 0);
    lv_obj_set_y(ui_data_temperature, 55);
    lv_obj_set_align(ui_data_temperature, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data_temperature, "25.0 " "\xE2\x84\x83");
    lv_obj_set_style_text_color(ui_data_temperature, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_temperature, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_data_temperature, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_temperature, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_humidity = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_humidity, 140);
    lv_obj_set_height(ui_Panel_humidity, 155);
    lv_obj_set_x(ui_Panel_humidity, 172);
    lv_obj_set_y(ui_Panel_humidity, 0);
    lv_obj_set_align(ui_Panel_humidity, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_clear_flag(ui_Panel_humidity, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_humidity, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    neiji_style_sensor_panel(ui_Panel_humidity);
    lv_obj_set_style_border_color(ui_Panel_humidity, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel_humidity, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_humidity, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_humidity, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_humidity, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_humidity, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_humidity = lv_img_create(ui_Panel_humidity);
    lv_img_set_src(ui_humidity, &ui_img_545021016);
    lv_obj_set_width(ui_humidity, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_humidity, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_humidity, 0);
    lv_obj_set_y(ui_humidity, 10);
    lv_obj_set_align(ui_humidity, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_humidity, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_humidity, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_data_humidity = lv_label_create(ui_Panel_humidity);
    lv_obj_set_width(ui_data_humidity, 120);
    lv_obj_set_height(ui_data_humidity, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_data_humidity, 0);
    lv_obj_set_y(ui_data_humidity, 55);
    lv_obj_set_align(ui_data_humidity, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data_humidity, "40.0 %");
    lv_obj_set_style_text_color(ui_data_humidity, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_humidity, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_data_humidity, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_humidity, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_baro = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_baro, 140);
    lv_obj_set_height(ui_Panel_baro, 155);
    lv_obj_set_x(ui_Panel_baro, 338);
    lv_obj_set_y(ui_Panel_baro, 0);
    lv_obj_set_align(ui_Panel_baro, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_clear_flag(ui_Panel_baro, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_baro, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    neiji_style_sensor_panel(ui_Panel_baro);
    lv_obj_set_style_border_color(ui_Panel_baro, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel_baro, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_baro, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_baro, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_baro, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_baro, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_baro = lv_img_create(ui_Panel_baro);
    lv_img_set_src(ui_baro, &ui_img_1523901040);
    lv_obj_set_width(ui_baro, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_baro, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_baro, 0);
    lv_obj_set_y(ui_baro, 10);
    lv_obj_set_align(ui_baro, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_baro, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_baro, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_data_baro = lv_label_create(ui_Panel_baro);
    lv_obj_set_width(ui_data_baro, 140);
    lv_obj_set_height(ui_data_baro, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_data_baro, 0);
    lv_obj_set_y(ui_data_baro, 55);
    lv_obj_set_align(ui_data_baro, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data_baro, "101.3 kPa");
    lv_obj_set_style_text_color(ui_data_baro, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_baro, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_data_baro, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_baro, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_CO2 = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_CO2, 140);
    lv_obj_set_height(ui_Panel_CO2, 155);
    lv_obj_set_x(ui_Panel_CO2, 504);
    lv_obj_set_y(ui_Panel_CO2, 0);
    lv_obj_set_align(ui_Panel_CO2, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_clear_flag(ui_Panel_CO2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_CO2, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    neiji_style_sensor_panel(ui_Panel_CO2);
    lv_obj_set_style_border_color(ui_Panel_CO2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel_CO2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_CO2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_CO2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_CO2, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_CO2, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_CO2 = lv_img_create(ui_Panel_CO2);
    lv_img_set_src(ui_CO2, &ui_img_1464373361);
    lv_obj_set_width(ui_CO2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_CO2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_CO2, 0);
    lv_obj_set_y(ui_CO2, 15);
    lv_obj_set_align(ui_CO2, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_CO2, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_CO2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_data_CO2 = lv_label_create(ui_Panel_CO2);
    lv_obj_set_width(ui_data_CO2, 140);
    lv_obj_set_height(ui_data_CO2, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_data_CO2, 0);
    lv_obj_set_y(ui_data_CO2, 55);
    lv_obj_set_align(ui_data_CO2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data_CO2, "400ppm");
    lv_obj_set_style_text_color(ui_data_CO2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_CO2, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_data_CO2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_CO2, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_PM2_5 = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_PM2_5, 140);
    lv_obj_set_height(ui_Panel_PM2_5, 155);
    lv_obj_set_x(ui_Panel_PM2_5, 670);
    lv_obj_set_y(ui_Panel_PM2_5, 0);
    lv_obj_set_align(ui_Panel_PM2_5, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_clear_flag(ui_Panel_PM2_5, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_PM2_5, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    neiji_style_sensor_panel(ui_Panel_PM2_5);
    lv_obj_set_style_border_color(ui_Panel_PM2_5, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel_PM2_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_PM2_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_PM2_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_PM2_5, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_PM2_5, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PM2_5 = lv_img_create(ui_Panel_PM2_5);
    lv_img_set_src(ui_PM2_5, &ui_img_pm2_5_png);
    lv_obj_set_width(ui_PM2_5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_PM2_5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_PM2_5, 0);
    lv_obj_set_y(ui_PM2_5, 15);
    lv_obj_set_align(ui_PM2_5, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_PM2_5, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_PM2_5, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_data_PM2_5 = lv_label_create(ui_Panel_PM2_5);
    lv_obj_set_width(ui_data_PM2_5, 140);
    lv_obj_set_height(ui_data_PM2_5, LV_SIZE_CONTENT);    /// 32
    lv_obj_set_x(ui_data_PM2_5, 0);
    lv_obj_set_y(ui_data_PM2_5, 55);
    lv_obj_set_align(ui_data_PM2_5, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data_PM2_5, "0 ug/m" "\xC2\xB3");
    lv_obj_set_style_text_color(ui_data_PM2_5, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_PM2_5, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_data_PM2_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_PM2_5, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_date = lv_label_create(ui_Main_Interface);
    lv_obj_set_width(ui_date, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_date, LV_SIZE_CONTENT);    /// 1
    lv_label_set_text(ui_date, "2025/01/01  00:00");
    lv_obj_set_style_text_color(ui_date, lv_color_hex(0xAEF7FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_date, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_date, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_on = lv_img_create(ui_Main_Interface);
    lv_img_set_src(ui_alarm_on, &ui_img_1985833602);
    lv_obj_set_width(ui_alarm_on, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_alarm_on, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_alarm_on, -50);
    lv_obj_set_y(ui_alarm_on, 0);
    lv_obj_set_align(ui_alarm_on, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_alarm_on, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_alarm_on, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_alarm_off = lv_img_create(ui_Main_Interface);
    lv_img_set_src(ui_alarm_off, &ui_img_1779444627);
    lv_obj_set_width(ui_alarm_off, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_alarm_off, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_alarm_off, -50);
    lv_obj_set_y(ui_alarm_off, 0);
    lv_obj_set_align(ui_alarm_off, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_alarm_off, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_alarm_off, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_led_on = lv_img_create(ui_Main_Interface);
    lv_img_set_src(ui_led_on, &ui_img_602667123);
    lv_obj_set_width(ui_led_on, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_led_on, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_led_on, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_led_on, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_led_on, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_led_off = lv_img_create(ui_Main_Interface);
    lv_img_set_src(ui_led_off, &ui_img_834997458);
    lv_obj_set_width(ui_led_off, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_led_off, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_led_off, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_led_off, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_led_off, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Panel_doserate = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_doserate, 814);
    lv_obj_set_height(ui_Panel_doserate, 199);
    lv_obj_set_x(ui_Panel_doserate, 0);
    lv_obj_set_y(ui_Panel_doserate, -181);
    lv_obj_set_align(ui_Panel_doserate, LV_ALIGN_BOTTOM_MID);
    lv_obj_clear_flag(ui_Panel_doserate, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_doserate, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    neiji_style_sensor_panel(ui_Panel_doserate);

    ui_radiation = lv_img_create(ui_Panel_doserate);
    lv_img_set_src(ui_radiation, &ui_img_1957260177);
    lv_obj_set_width(ui_radiation, 100);
    lv_obj_set_height(ui_radiation, 100);
    lv_obj_set_align(ui_radiation, LV_ALIGN_RIGHT_MID);
    lv_obj_add_flag(ui_radiation, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_radiation, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_img_set_pivot(ui_radiation, 50, 50);

    ui_data_doserate = lv_label_create(ui_Panel_doserate);
    lv_obj_set_width(ui_data_doserate, 425);
    lv_obj_set_height(ui_data_doserate, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_data_doserate, 10);
    lv_obj_set_y(ui_data_doserate, 12);
    lv_obj_set_align(ui_data_doserate, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_data_doserate, "0.00");
    lv_obj_set_style_text_color(ui_data_doserate, lv_color_hex(0xFDBB5E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_doserate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_data_doserate, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_doserate, &ui_font_hansanbold128, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_data_unit = lv_label_create(ui_Panel_doserate);
    lv_obj_set_width(ui_data_unit, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_data_unit, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_data_unit, 162);
    lv_obj_set_y(ui_data_unit, 36);
    lv_obj_set_align(ui_data_unit, LV_ALIGN_CENTER);
    lv_label_set_text(ui_data_unit, "\xCE\xBC" "Sv/h");
    lv_obj_set_style_text_color(ui_data_unit, lv_color_hex(0x61D2FD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_data_unit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_data_unit, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_MainMenu1 = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_MainMenu1, 854);
    lv_obj_set_height(ui_MainMenu1, 480);
    lv_obj_set_align(ui_MainMenu1, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_MainMenu1, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CHECKABLE);     /// Flags
    lv_obj_clear_flag(ui_MainMenu1, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_GESTURE_BUBBLE |
                        LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_MainMenu1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MainMenu1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MainMenu1, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_MainMenu1, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_MainMenu1, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_MainMenu1, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_MainMenu1, 20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_sidebar = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_sidebar, 80);
    lv_obj_set_height(ui_sidebar, 345);
    lv_obj_set_align(ui_sidebar, LV_ALIGN_RIGHT_MID);
    lv_obj_clear_flag(ui_sidebar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_sidebar, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_sidebar, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_sidebar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_sidebar, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_sidebar, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_sidebar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_sidebar, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_setting = lv_btn_create(ui_sidebar);
    lv_obj_set_width(ui_alarm_setting, 55);
    lv_obj_set_height(ui_alarm_setting, 55);
    lv_obj_set_x(ui_alarm_setting, 0);
    lv_obj_set_y(ui_alarm_setting, 21);
    lv_obj_set_align(ui_alarm_setting, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_alarm_setting, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_alarm_setting, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_alarm_setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alarm_setting, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_alarm_setting, &ui_img_1777030953, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_alarm_setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_bg_opa(ui_alarm_setting, 50, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_img_src(ui_alarm_setting, &ui_img_1777030953, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui_alarm_setting, lv_color_hex(0x7EFEFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_alarm_setting, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ui_alarm_setting, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(ui_alarm_setting, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(ui_alarm_setting, 0, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_display_setting = lv_btn_create(ui_sidebar);
    lv_obj_set_width(ui_display_setting, 55);
    lv_obj_set_height(ui_display_setting, 55);
    lv_obj_set_x(ui_display_setting, 0);
    lv_obj_set_y(ui_display_setting, 97);
    lv_obj_set_align(ui_display_setting, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_display_setting, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_display_setting, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_display_setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_display_setting, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_display_setting, &ui_img_display_png, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_display_setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_bg_opa(ui_display_setting, 50, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_img_src(ui_display_setting, &ui_img_display_png, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui_display_setting, lv_color_hex(0x7EFEFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_display_setting, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ui_display_setting, 3, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_datetime_setting = lv_btn_create(ui_sidebar);
    lv_obj_set_width(ui_datetime_setting, 55);
    lv_obj_set_height(ui_datetime_setting, 55);
    lv_obj_set_x(ui_datetime_setting, 0);
    lv_obj_set_y(ui_datetime_setting, 173);
    lv_obj_set_align(ui_datetime_setting, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_datetime_setting, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_datetime_setting, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_datetime_setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_datetime_setting, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_datetime_setting, &ui_img_2138131786, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_datetime_setting, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_bg_opa(ui_datetime_setting, 50, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_img_src(ui_datetime_setting, &ui_img_2138131786, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui_datetime_setting, lv_color_hex(0x7EFEFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_datetime_setting, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ui_datetime_setting, 3, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_about = lv_btn_create(ui_sidebar);
    lv_obj_set_width(ui_about, 55);
    lv_obj_set_height(ui_about, 55);
    lv_obj_set_x(ui_about, 0);
    lv_obj_set_y(ui_about, 249);
    lv_obj_set_align(ui_about, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(ui_about, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_about, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_about, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_about, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_about, &ui_img_182239421, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_about, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    // lv_obj_set_style_bg_opa(ui_about, 50, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_img_src(ui_about, &ui_img_182239421, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(ui_about, lv_color_hex(0x7EFEFF), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(ui_about, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ui_about, 3, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_Panel_alarm_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_alarm_set, 640);
    lv_obj_set_height(ui_Panel_alarm_set, 347);
    lv_obj_set_x(ui_Panel_alarm_set, -94);
    lv_obj_set_y(ui_Panel_alarm_set, 46);
    lv_obj_set_align(ui_Panel_alarm_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_alarm_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_alarm_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_alarm_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_alarm_set, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_alarm_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_alarm_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_alarm_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_alarm_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_alarm_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label12 = lv_label_create(ui_Panel_alarm_set);
    lv_obj_set_width(ui_Label12, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label12, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label12, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label12, "\xE6\x8A\xA5\xE8\xAD\xA6\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label12, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label12, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label12, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_light_sw = lv_btn_create(ui_Panel_alarm_set);
    lv_obj_set_width(ui_alarm_light_sw, 460);
    lv_obj_set_height(ui_alarm_light_sw, 60);
    lv_obj_set_x(ui_alarm_light_sw, -7);
    lv_obj_set_y(ui_alarm_light_sw, 60);
    lv_obj_set_align(ui_alarm_light_sw, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_alarm_light_sw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_alarm_light_sw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_alarm_light_sw, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_alarm_light_sw, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_alarm_light_sw, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alarm_light_sw, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_alarm_light_sw, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_alarm_light_sw, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label11 = lv_label_create(ui_alarm_light_sw);
    lv_obj_set_width(ui_Label11, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label11, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label11, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label11, "\xE7\x81\xAF\xE5\x85\x89\xE6\x8A\xA5\xE8\xAD\xA6\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_Label11, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label11, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label11, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_light_alarm_state = lv_label_create(ui_alarm_light_sw);
    lv_obj_set_width(ui_light_alarm_state, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_light_alarm_state, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_light_alarm_state, 110);
    lv_obj_set_y(ui_light_alarm_state, 0);
    lv_obj_set_align(ui_light_alarm_state, LV_ALIGN_CENTER);
    lv_label_set_text(ui_light_alarm_state, "\xE5\xBC\x80");
    lv_obj_set_style_text_color(ui_light_alarm_state, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_light_alarm_state, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_light_alarm_state, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_beep_sw = lv_btn_create(ui_Panel_alarm_set);
    lv_obj_set_width(ui_alarm_beep_sw, 460);
    lv_obj_set_height(ui_alarm_beep_sw, 60);
    lv_obj_set_x(ui_alarm_beep_sw, 0);
    lv_obj_set_y(ui_alarm_beep_sw, 125);
    lv_obj_set_align(ui_alarm_beep_sw, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_alarm_beep_sw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_alarm_beep_sw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_alarm_beep_sw, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_alarm_beep_sw, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_alarm_beep_sw, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_alarm_beep_sw, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_alarm_beep_sw, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_alarm_beep_sw, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_Label21 = lv_label_create(ui_alarm_beep_sw);
    lv_obj_set_width(ui_Label21, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label21, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label21, -26);
    lv_obj_set_y(ui_Label21, 68);
    lv_obj_set_align(ui_Label21, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label21, "\xE8\x9C\x82\xE9\xB8\xA3\xE5\x99\xA8\xE6\x8A\xA5\xE8\xAD\xA6\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_Label21, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label21, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label21, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_alarm_volume = lv_label_create(ui_alarm_beep_sw);
    lv_obj_set_width(ui_alarm_volume, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_alarm_volume, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_alarm_volume, 250);
    lv_obj_set_y(ui_alarm_volume, 132);
    lv_obj_set_align(ui_alarm_volume, LV_ALIGN_CENTER);
    lv_label_set_text(ui_alarm_volume, "75 %");
    lv_obj_set_style_text_color(ui_alarm_volume, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_alarm_volume, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_alarm_volume, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Hth_Set_Btn = lv_btn_create(ui_Panel_alarm_set);
    lv_obj_set_width(ui_Hth_Set_Btn, 460);
    lv_obj_set_height(ui_Hth_Set_Btn, 60);
    lv_obj_set_x(ui_Hth_Set_Btn, 0);
    lv_obj_set_y(ui_Hth_Set_Btn, 190);
    lv_obj_set_align(ui_Hth_Set_Btn, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_Hth_Set_Btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_Hth_Set_Btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_Hth_Set_Btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Hth_Set_Btn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Hth_Set_Btn, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Hth_Set_Btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Hth_Set_Btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_Hth_Set_Btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_Label19 = lv_label_create(ui_Hth_Set_Btn);
    lv_obj_set_width(ui_Label19, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label19, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label19, -27);
    lv_obj_set_y(ui_Label19, 68);
    lv_obj_set_align(ui_Label19, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label19, "\xE9\xAB\x98\xE4\xBD\x8D\xE9\x98\x88\xE5\x80\xBC\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_Label19, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label19, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label19, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_high_threshold = lv_label_create(ui_Hth_Set_Btn);
    lv_obj_set_width(ui_high_threshold, LV_SIZE_CONTENT);   /// 200
    lv_obj_set_height(ui_high_threshold, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_high_threshold, 212);
    lv_obj_set_y(ui_high_threshold, 197);
    lv_label_set_text(ui_high_threshold, "2.50 " "\xCE\xBC" "Sv/h");
    lv_obj_set_style_text_color(ui_high_threshold, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_high_threshold, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_high_threshold, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Lth_Set_Btn = lv_btn_create(ui_Panel_alarm_set);
    lv_obj_set_width(ui_Lth_Set_Btn, 460);
    lv_obj_set_height(ui_Lth_Set_Btn, 60);
    lv_obj_set_x(ui_Lth_Set_Btn, 0);
    lv_obj_set_y(ui_Lth_Set_Btn, 255);
    lv_obj_set_align(ui_Lth_Set_Btn, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_Lth_Set_Btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_Lth_Set_Btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_Lth_Set_Btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Lth_Set_Btn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Lth_Set_Btn, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Lth_Set_Btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Lth_Set_Btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_Lth_Set_Btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_Label22 = lv_label_create(ui_Lth_Set_Btn);
    lv_obj_set_width(ui_Label22, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label22, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label22, -27);
    lv_obj_set_y(ui_Label22, 68);
    lv_obj_set_align(ui_Label22, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label22, "\xE4\xBD\x8E\xE4\xBD\x8D\xE9\x98\x88\xE5\x80\xBC\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_Label22, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label22, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label22, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_low_threshold = lv_label_create(ui_Lth_Set_Btn);
    lv_obj_set_width(ui_low_threshold, LV_SIZE_CONTENT);   /// 200
    lv_obj_set_height(ui_low_threshold, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_low_threshold, 212);
    lv_obj_set_y(ui_low_threshold, 262);
    lv_label_set_text(ui_low_threshold, "0.00 " "\xCE\xBC" "Sv/h");
    lv_obj_set_style_text_color(ui_low_threshold, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_low_threshold, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_low_threshold, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_volume_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_volume_set, 640);
    lv_obj_set_height(ui_Panel_volume_set, 347);
    lv_obj_set_x(ui_Panel_volume_set, -94);
    lv_obj_set_y(ui_Panel_volume_set, 46);
    lv_obj_set_align(ui_Panel_volume_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_volume_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_volume_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_volume_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_volume_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_volume_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_volume_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_volume_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_volume_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_volume_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_set_alarm_volume = lv_label_create(ui_Panel_volume_set);
    lv_obj_set_width(ui_set_alarm_volume, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_set_alarm_volume, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_set_alarm_volume, 0);
    lv_obj_set_y(ui_set_alarm_volume, 80);
    lv_obj_set_align(ui_set_alarm_volume, LV_ALIGN_CENTER);
    lv_label_set_text(ui_set_alarm_volume, "75 %");
    lv_obj_add_flag(ui_set_alarm_volume, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_set_alarm_volume, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_set_alarm_volume, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_set_alarm_volume, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label8 = lv_label_create(ui_Panel_volume_set);
    lv_obj_set_width(ui_Label8, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label8, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label8, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label8, "\xE6\x8A\xA5\xE8\xAD\xA6\xE9\x9F\xB3\xE9\x87\x8F\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label8, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label8, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label8, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Slider_volume = lv_slider_create(ui_Panel_volume_set);
    lv_slider_set_value(ui_Slider_volume, 75, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_Slider_volume) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_Slider_volume, 0,
                                                                                                LV_ANIM_OFF);
    lv_obj_set_width(ui_Slider_volume, 480);
    lv_obj_set_height(ui_Slider_volume, 20);
    lv_obj_set_x(ui_Slider_volume, 65);
    lv_obj_set_y(ui_Slider_volume, 123);
    lv_obj_set_style_bg_color(ui_Slider_volume, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider_volume, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_Slider_volume, lv_color_hex(0xBCEAEB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_Slider_volume, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Slider_volume, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Slider_volume, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Slider_volume, lv_color_hex(0x7EFEFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider_volume, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Slider_volume, lv_color_hex(0xE0E0E0), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_Slider_volume, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_Slider_volume, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_Slider_volume, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_Slider_volume, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_Slider_volume, 168, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Slider_volume, 1, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Slider_volume, 2, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_Slider_volume, 0, LV_PART_INDICATOR | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(ui_Slider_volume, 0, LV_PART_INDICATOR | LV_STATE_FOCUSED);

    lv_obj_set_style_bg_color(ui_Slider_volume, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider_volume, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_Panel_high_th_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_high_th_set, 640);
    lv_obj_set_height(ui_Panel_high_th_set, 347);
    lv_obj_set_x(ui_Panel_high_th_set, -94);
    lv_obj_set_y(ui_Panel_high_th_set, 46);
    lv_obj_set_align(ui_Panel_high_th_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_high_th_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_high_th_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_high_th_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_high_th_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_high_th_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_high_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_high_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_high_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_high_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label2 = lv_label_create(ui_Panel_high_th_set);
    lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label2, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label2, "\xE9\xAB\x98\xE4\xBD\x8D\xE9\x98\x88\xE5\x80\xBC\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label2, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label2, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_HthLeft3 = lv_roller_create(ui_Panel_high_th_set);
    lv_roller_set_options(ui_HthLeft3, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_HthLeft3, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_HthLeft3, 45);
    lv_obj_set_height(ui_HthLeft3, 64);
    lv_obj_set_x(ui_HthLeft3, 32);
    lv_obj_set_y(ui_HthLeft3, 15);
    lv_obj_set_align(ui_HthLeft3, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_HthLeft3, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthLeft3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_HthLeft3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_HthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_HthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_HthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_HthLeft3, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_HthLeft3, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_HthLeft3, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthLeft3, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthLeft3, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_HthLeft3, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_HthLeft3, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_HthLeft2 = lv_roller_create(ui_Panel_high_th_set);
    lv_roller_set_options(ui_HthLeft2, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_HthLeft2, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_HthLeft2, 45);
    lv_obj_set_height(ui_HthLeft2, 64);
    lv_obj_set_x(ui_HthLeft2, 92);
    lv_obj_set_y(ui_HthLeft2, 15);
    lv_obj_set_align(ui_HthLeft2, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_HthLeft2, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthLeft2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_HthLeft2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_HthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_HthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_HthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_HthLeft2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_HthLeft2, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_HthLeft2, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthLeft2, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthLeft2, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_HthLeft2, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_HthLeft2, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_HthLeft1 = lv_roller_create(ui_Panel_high_th_set);
    lv_roller_set_options(ui_HthLeft1, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_HthLeft1, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_HthLeft1, 45);
    lv_obj_set_height(ui_HthLeft1, 64);
    lv_obj_set_x(ui_HthLeft1, 152);
    lv_obj_set_y(ui_HthLeft1, 15);
    lv_obj_set_align(ui_HthLeft1, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_HthLeft1, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthLeft1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_HthLeft1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_HthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_HthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_HthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_HthLeft1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_HthLeft1, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_HthLeft1, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthLeft1, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthLeft1, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_HthLeft1, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_HthLeft1, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_HthRight1 = lv_roller_create(ui_Panel_high_th_set);
    lv_roller_set_options(ui_HthRight1, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_HthRight1, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_HthRight1, 45);
    lv_obj_set_height(ui_HthRight1, 64);
    lv_obj_set_x(ui_HthRight1, 252);
    lv_obj_set_y(ui_HthRight1, 15);
    lv_obj_set_align(ui_HthRight1, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_HthRight1, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthRight1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_HthRight1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_HthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_HthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_HthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_HthRight1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_HthRight1, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_HthRight1, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthRight1, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthRight1, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_HthRight1, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_HthRight1, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_HthRight2 = lv_roller_create(ui_Panel_high_th_set);
    lv_roller_set_options(ui_HthRight2, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_HthRight2, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_HthRight2, 45);
    lv_obj_set_height(ui_HthRight2, 64);
    lv_obj_set_x(ui_HthRight2, 312);
    lv_obj_set_y(ui_HthRight2, 15);
    lv_obj_set_align(ui_HthRight2, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_HthRight2, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthRight2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_HthRight2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_HthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_HthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_HthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_HthRight2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_HthRight2, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_HthRight2, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthRight2, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthRight2, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_HthRight2, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_HthRight2, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_HthUnit = lv_roller_create(ui_Panel_high_th_set);
    lv_roller_set_options(ui_HthUnit, "mSv/h\n" "\xCE\xBC" "Sv/h", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_HthUnit, 1, LV_ANIM_OFF);
    lv_obj_set_width(ui_HthUnit, 195);
    lv_obj_set_height(ui_HthUnit, 64);
    lv_obj_set_x(ui_HthUnit, 372);
    lv_obj_set_y(ui_HthUnit, 15);
    lv_obj_set_align(ui_HthUnit, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_HthUnit, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthUnit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_HthUnit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_HthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_HthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_HthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_HthUnit, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_HthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_HthUnit, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_HthUnit, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_HthUnit, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HthUnit, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_HthUnit, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_HthUnit, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_Label1 = lv_label_create(ui_Panel_high_th_set);
    lv_obj_set_width(ui_Label1, 45);
    lv_obj_set_height(ui_Label1, 100);
    lv_obj_set_x(ui_Label1, 202);
    lv_obj_set_y(ui_Label1, 15);
    lv_obj_set_align(ui_Label1, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_Label1, ".");
    lv_obj_add_flag(ui_Label1, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Label1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label1, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Label1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Label1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Label1, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Label1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_low_th_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_low_th_set, 640);
    lv_obj_set_height(ui_Panel_low_th_set, 347);
    lv_obj_set_x(ui_Panel_low_th_set, -94);
    lv_obj_set_y(ui_Panel_low_th_set, 46);
    lv_obj_set_align(ui_Panel_low_th_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_low_th_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_low_th_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_low_th_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_low_th_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_low_th_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_low_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_low_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_low_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_low_th_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label5 = lv_label_create(ui_Panel_low_th_set);
    lv_obj_set_width(ui_Label5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label5, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label5, "\xE4\xBD\x8E\xE4\xBD\x8D\xE9\x98\x88\xE5\x80\xBC\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label5, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label5, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LthLeft3 = lv_roller_create(ui_Panel_low_th_set);
    lv_roller_set_options(ui_LthLeft3, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_LthLeft3, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_LthLeft3, 45);
    lv_obj_set_height(ui_LthLeft3, 64);
    lv_obj_set_x(ui_LthLeft3, 32);
    lv_obj_set_y(ui_LthLeft3, 15);
    lv_obj_set_align(ui_LthLeft3, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_LthLeft3, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthLeft3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LthLeft3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_LthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_LthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_LthLeft3, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_LthLeft3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_LthLeft3, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LthLeft3, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthLeft3, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthLeft3, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LthLeft3, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_LthLeft3, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_LthLeft2 = lv_roller_create(ui_Panel_low_th_set);
    lv_roller_set_options(ui_LthLeft2, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_LthLeft2, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_LthLeft2, 45);
    lv_obj_set_height(ui_LthLeft2, 64);
    lv_obj_set_x(ui_LthLeft2, 92);
    lv_obj_set_y(ui_LthLeft2, 15);
    lv_obj_set_align(ui_LthLeft2, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_LthLeft2, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthLeft2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LthLeft2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_LthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_LthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_LthLeft2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_LthLeft2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_LthLeft2, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LthLeft2, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthLeft2, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthLeft2, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LthLeft2, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_LthLeft2, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_LthLeft1 = lv_roller_create(ui_Panel_low_th_set);
    lv_roller_set_options(ui_LthLeft1, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_LthLeft1, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_LthLeft1, 45);
    lv_obj_set_height(ui_LthLeft1, 64);
    lv_obj_set_x(ui_LthLeft1, 152);
    lv_obj_set_y(ui_LthLeft1, 15);
    lv_obj_set_align(ui_LthLeft1, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_LthLeft1, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthLeft1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LthLeft1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_LthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_LthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_LthLeft1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_LthLeft1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_LthLeft1, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LthLeft1, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthLeft1, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthLeft1, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LthLeft1, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_LthLeft1, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_LthRight1 = lv_roller_create(ui_Panel_low_th_set);
    lv_roller_set_options(ui_LthRight1, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_LthRight1, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_LthRight1, 45);
    lv_obj_set_height(ui_LthRight1, 64);
    lv_obj_set_x(ui_LthRight1, 252);
    lv_obj_set_y(ui_LthRight1, 15);
    lv_obj_set_align(ui_LthRight1, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_LthRight1, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthRight1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LthRight1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_LthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_LthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_LthRight1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_LthRight1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_LthRight1, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LthRight1, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthRight1, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthRight1, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LthRight1, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_LthRight1, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_LthRight2 = lv_roller_create(ui_Panel_low_th_set);
    lv_roller_set_options(ui_LthRight2, "9\n8\n7\n6\n5\n4\n3\n2\n1\n0", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_LthRight2, 9, LV_ANIM_OFF);
    lv_obj_set_width(ui_LthRight2, 45);
    lv_obj_set_height(ui_LthRight2, 64);
    lv_obj_set_x(ui_LthRight2, 312);
    lv_obj_set_y(ui_LthRight2, 15);
    lv_obj_set_align(ui_LthRight2, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_LthRight2, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthRight2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LthRight2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_LthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_LthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_LthRight2, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_LthRight2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_LthRight2, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LthRight2, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthRight2, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthRight2, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LthRight2, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_LthRight2, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_LthUnit = lv_roller_create(ui_Panel_low_th_set);
    lv_roller_set_options(ui_LthUnit, "mSv/h\n" "\xCE\xBC" "Sv/h", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(ui_LthUnit, 1, LV_ANIM_OFF);
    lv_obj_set_width(ui_LthUnit, 195);
    lv_obj_set_height(ui_LthUnit, 64);
    lv_obj_set_x(ui_LthUnit, 372);
    lv_obj_set_y(ui_LthUnit, 15);
    lv_obj_set_align(ui_LthUnit, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_LthUnit, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthUnit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_LthUnit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_LthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_LthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_LthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_LthUnit, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_LthUnit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_LthUnit, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LthUnit, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_LthUnit, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LthUnit, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_LthUnit, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_LthUnit, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_Label3 = lv_label_create(ui_Panel_low_th_set);
    lv_obj_set_width(ui_Label3, 45);
    lv_obj_set_height(ui_Label3, 100);
    lv_obj_set_x(ui_Label3, 202);
    lv_obj_set_y(ui_Label3, 15);
    lv_obj_set_align(ui_Label3, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_Label3, ".");
    lv_obj_add_flag(ui_Label3, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_Label3, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Label3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label3, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Label3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Label3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Label3, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Label3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_display_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_display_set, 640);
    lv_obj_set_height(ui_Panel_display_set, 347);
    lv_obj_set_x(ui_Panel_display_set, -94);
    lv_obj_set_y(ui_Panel_display_set, 46);
    lv_obj_set_align(ui_Panel_display_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_display_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_display_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_display_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_display_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_display_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_display_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_display_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_display_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_display_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label7 = lv_label_create(ui_Panel_display_set);
    lv_obj_set_width(ui_Label7, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label7, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label7, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label7, "\xE6\x98\xBE\xE7\xA4\xBA\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label7, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label7, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_language_sw = lv_btn_create(ui_Panel_display_set);
    lv_obj_set_width(ui_language_sw, 460);
    lv_obj_set_height(ui_language_sw, 60);
    lv_obj_set_x(ui_language_sw, 0);
    lv_obj_set_y(ui_language_sw, 100);
    lv_obj_set_align(ui_language_sw, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_language_sw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_language_sw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_language_sw, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_language_sw, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_language_sw, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_language_sw, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label18 = lv_label_create(ui_language_sw);
    lv_obj_set_width(ui_Label18, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label18, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label18, -32);
    lv_obj_set_y(ui_Label18, 6);
    lv_obj_set_align(ui_Label18, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label18, "\xE7\xB3\xBB\xE7\xBB\x9F\xE8\xAF\xAD\xE8\xA8\x80\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_Label18, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label18, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label18, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_sys_language = lv_label_create(ui_language_sw);
    lv_obj_set_width(ui_sys_language, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_sys_language, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_sys_language, 169);
    lv_obj_set_y(ui_sys_language, 101);
    lv_label_set_text(ui_sys_language, "\xE4\xB8\xAD\xE6\x96\x87");
    lv_obj_set_style_text_color(ui_sys_language, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_sys_language, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_sys_language, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_sys_language, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_screen_light_sw = lv_btn_create(ui_Panel_display_set);
    lv_obj_set_width(ui_screen_light_sw, 460);
    lv_obj_set_height(ui_screen_light_sw, 60);
    lv_obj_set_x(ui_screen_light_sw, 0);
    lv_obj_set_y(ui_screen_light_sw, 200);
    lv_obj_set_align(ui_screen_light_sw, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_screen_light_sw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_screen_light_sw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_screen_light_sw, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_screen_light_sw, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_screen_light_sw, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_screen_light_sw, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_screen_light_sw, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_screen_light_sw, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_Label16 = lv_label_create(ui_screen_light_sw);
    lv_obj_set_width(ui_Label16, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label16, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label16, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label16, "\xE5\xB1\x8F\xE5\xB9\x95\xE4\xBA\xAE\xE5\xBA\xA6\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_Label16, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label16, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label16, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_sys_bright = lv_label_create(ui_screen_light_sw);
    lv_obj_set_width(ui_sys_bright, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_sys_bright, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_sys_bright, 168);
    lv_obj_set_y(ui_sys_bright, 200);
    lv_label_set_text(ui_sys_bright, "75 %");
    lv_obj_set_style_text_color(ui_sys_bright, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_sys_bright, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_sys_bright, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_sys_bright, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_language_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_language_set, 640);
    lv_obj_set_height(ui_Panel_language_set, 347);
    lv_obj_set_x(ui_Panel_language_set, -94);
    lv_obj_set_y(ui_Panel_language_set, 46);
    lv_obj_set_align(ui_Panel_language_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_language_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_language_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_language_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_language_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_language_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_language_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_language_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_language_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_language_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label17 = lv_label_create(ui_Panel_language_set);
    lv_obj_set_width(ui_Label17, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label17, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label17, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label17, "\xE7\xB3\xBB\xE7\xBB\x9F\xE8\xAF\xAD\xE8\xA8\x80\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label17, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label17, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label17, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LanguageChineseBtn = lv_btn_create(ui_Panel_language_set);
    lv_obj_set_width(ui_LanguageChineseBtn, 400);
    lv_obj_set_height(ui_LanguageChineseBtn, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LanguageChineseBtn, 0);
    lv_obj_set_y(ui_LanguageChineseBtn, -15);
    lv_obj_set_align(ui_LanguageChineseBtn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_LanguageChineseBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_LanguageChineseBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_LanguageChineseBtn, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LanguageChineseBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_labelChinese = lv_label_create(ui_LanguageChineseBtn);
    lv_obj_set_width(ui_labelChinese, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_labelChinese, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_labelChinese, LV_ALIGN_CENTER);
    lv_label_set_text(ui_labelChinese, "\xE4\xB8\xAD\xE6\x96\x87");
    lv_obj_set_style_text_color(ui_labelChinese, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_labelChinese, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_labelChinese, &ui_font_hansanbold36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LanguageEnglishBtn = lv_btn_create(ui_Panel_language_set);
    lv_obj_set_width(ui_LanguageEnglishBtn, 400);
    lv_obj_set_height(ui_LanguageEnglishBtn, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LanguageEnglishBtn, 0);
    lv_obj_set_y(ui_LanguageEnglishBtn, 75);
    lv_obj_set_align(ui_LanguageEnglishBtn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_LanguageEnglishBtn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_LanguageEnglishBtn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_LanguageEnglishBtn, lv_color_hex(0x292831), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LanguageEnglishBtn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_labelEnglish = lv_label_create(ui_LanguageEnglishBtn);
    lv_obj_set_width(ui_labelEnglish, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_labelEnglish, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_labelEnglish, LV_ALIGN_CENTER);
    lv_label_set_text(ui_labelEnglish, "English");
    lv_obj_set_style_text_color(ui_labelEnglish, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_labelEnglish, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_labelEnglish, &ui_font_hansanbold36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_bright_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_bright_set, 640);
    lv_obj_set_height(ui_Panel_bright_set, 347);
    lv_obj_set_x(ui_Panel_bright_set, -94);
    lv_obj_set_y(ui_Panel_bright_set, 46);
    lv_obj_set_align(ui_Panel_bright_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_bright_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_bright_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_bright_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_bright_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_bright_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_bright_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_bright_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_bright_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_bright_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_set_bright = lv_label_create(ui_Panel_bright_set);
    lv_obj_set_width(ui_set_bright, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_set_bright, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_set_bright, 0);
    lv_obj_set_y(ui_set_bright, 80);
    lv_obj_set_align(ui_set_bright, LV_ALIGN_CENTER);
    lv_label_set_text(ui_set_bright, "75 %");
    lv_obj_add_flag(ui_set_bright, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_set_bright, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_set_bright, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_set_bright, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label10 = lv_label_create(ui_Panel_bright_set);
    lv_obj_set_width(ui_Label10, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label10, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label10, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label10, "\xE5\xB1\x8F\xE5\xB9\x95\xE4\xBA\xAE\xE5\xBA\xA6\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label10, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label10, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Slider_bright = lv_slider_create(ui_Panel_bright_set);
    lv_slider_set_range(ui_Slider_bright, 1, 100);
    lv_slider_set_value(ui_Slider_bright, 100, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_Slider_bright) == LV_SLIDER_MODE_RANGE) 
        lv_slider_set_left_value(ui_Slider_bright, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_Slider_bright, 480);
    lv_obj_set_height(ui_Slider_bright, 20);
    lv_obj_set_x(ui_Slider_bright, 65);
    lv_obj_set_y(ui_Slider_bright, 123);
    lv_obj_set_style_bg_color(ui_Slider_bright, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider_bright, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_Slider_bright, lv_color_hex(0xBCEAEB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_Slider_bright, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Slider_bright, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Slider_bright, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_x(ui_Slider_bright, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_ofs_y(ui_Slider_bright, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Slider_bright, lv_color_hex(0x7EFEFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider_bright, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Slider_bright, lv_color_hex(0xE0E0E0), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_Slider_bright, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_Slider_bright, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_Slider_bright, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(ui_Slider_bright, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(ui_Slider_bright, 168, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Slider_bright, 1, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Slider_bright, 2, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Slider_bright, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider_bright, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_Panel_datetime_set = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_datetime_set, 640);
    lv_obj_set_height(ui_Panel_datetime_set, 347);
    lv_obj_set_x(ui_Panel_datetime_set, -94);
    lv_obj_set_y(ui_Panel_datetime_set, 46);
    lv_obj_set_align(ui_Panel_datetime_set, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_datetime_set, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_datetime_set, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_datetime_set, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_datetime_set, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_datetime_set, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_datetime_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_datetime_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_datetime_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_datetime_set, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label14 = lv_label_create(ui_Panel_datetime_set);
    lv_obj_set_width(ui_Label14, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label14, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label14, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label14, "\xE6\x97\xA5\xE6\x9C\x9F\xE5\x92\x8C\xE6\x97\xB6\xE9\x97\xB4\xE8\xAE\xBE\xE7\xBD\xAE");
    lv_obj_set_style_text_color(ui_Label14, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label14, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label14, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Year = lv_roller_create(ui_Panel_datetime_set);
    lv_roller_set_options(ui_Year,
                            "99\n98\n97\n96\n95\n94\n93\n92\n91\n90\n89\n88\n87\n86\n85\n84\n83\n82\n81\n80\n79\n78\n77\n76\n75\n74\n73\n72\n71\n70\n69\n68\n67\n66\n65\n64\n63\n62\n61\n60\n59\n58\n57\n56\n55\n54\n53\n52\n51\n50\n49\n48\n47\n46\n45\n44\n43\n42\n41\n40\n39\n38\n37\n36\n35\n34\n33\n32\n31\n30\n29\n28\n27\n26\n25\n24\n23\n22\n21\n20\n19\n18\n17\n16\n15\n14\n13\n12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01\n00",
                            LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_Year, 74, LV_ANIM_OFF);
    lv_obj_set_width(ui_Year, 72);
    lv_obj_set_height(ui_Year, 64);
    lv_obj_set_x(ui_Year, 25);
    lv_obj_set_y(ui_Year, 10);
    lv_obj_set_align(ui_Year, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_Year, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Year, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Year, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Year, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Year, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Year, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Year, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Year, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Year, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_Year, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Year, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Year, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Year, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_Year, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_Year, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_month = lv_roller_create(ui_Panel_datetime_set);
    lv_roller_set_options(ui_month, "12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_month, 11, LV_ANIM_OFF);
    lv_obj_set_width(ui_month, 72);
    lv_obj_set_height(ui_month, 64);
    lv_obj_set_x(ui_month, 150);
    lv_obj_set_y(ui_month, 10);
    lv_obj_set_align(ui_month, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_month, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_month, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_month, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_month, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_month, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_month, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_month, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_month, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_month, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_month, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_month, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_month, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_month, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_month, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_month, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_day = lv_roller_create(ui_Panel_datetime_set);
    lv_roller_set_options(ui_day,
                            "31\n30\n29\n28\n27\n26\n25\n24\n23\n22\n21\n20\n19\n18\n17\n16\n15\n14\n13\n12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01",
                            LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_day, 30, LV_ANIM_OFF);
    lv_obj_set_width(ui_day, 72);
    lv_obj_set_height(ui_day, 64);
    lv_obj_set_x(ui_day, 275);
    lv_obj_set_y(ui_day, 10);
    lv_obj_set_align(ui_day, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_day, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_day, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_day, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_day, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_day, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_day, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_day, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_day, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_day, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_day, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_day, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_day, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_day, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_day, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_day, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_hour = lv_roller_create(ui_Panel_datetime_set);
    lv_roller_set_options(ui_hour,
                            "23\n22\n21\n20\n19\n18\n17\n16\n15\n14\n13\n12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01\n00",
                            LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_hour, 11, LV_ANIM_OFF);
    lv_obj_set_width(ui_hour, 72);
    lv_obj_set_height(ui_hour, 64);
    lv_obj_set_x(ui_hour, 372);
    lv_obj_set_y(ui_hour, 10);
    lv_obj_set_align(ui_hour, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_hour, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_hour, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_hour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_hour, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_hour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_hour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_hour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_hour, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_hour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_hour, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_hour, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_hour, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_hour, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_hour, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_hour, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_minute = lv_roller_create(ui_Panel_datetime_set);
    lv_roller_set_options(ui_minute,
                            "59\n58\n57\n56\n55\n54\n53\n52\n51\n50\n49\n48\n47\n46\n45\n44\n43\n42\n41\n40\n39\n38\n37\n36\n35\n34\n33\n32\n31\n30\n29\n28\n27\n26\n25\n24\n23\n22\n21\n20\n19\n18\n17\n16\n15\n14\n13\n12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01\n00",
                            LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(ui_minute, 59, LV_ANIM_OFF);
    lv_obj_set_width(ui_minute, 72);
    lv_obj_set_height(ui_minute, 64);
    lv_obj_set_x(ui_minute, 497);
    lv_obj_set_y(ui_minute, 10);
    lv_obj_set_align(ui_minute, LV_ALIGN_LEFT_MID);
    lv_obj_set_style_text_font(ui_minute, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_minute, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_minute, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_minute, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_minute, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_minute, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_minute, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_minute, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_minute, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(ui_minute, lv_color_hex(0xC6E1FF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_minute, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_minute, lv_color_hex(0xFFFFFF), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_minute, 0, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_minute, lv_color_hex(0x7EFEFF), LV_PART_SELECTED | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_minute, 255, LV_PART_SELECTED | LV_STATE_FOCUSED);

    ui_Label4 = lv_label_create(ui_Panel_datetime_set);
    lv_obj_set_width(ui_Label4, 45);
    lv_obj_set_height(ui_Label4, 100);
    lv_obj_set_x(ui_Label4, 100);
    lv_obj_set_y(ui_Label4, 15);
    lv_obj_set_align(ui_Label4, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_Label4, "/");
    lv_obj_add_flag(ui_Label4, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_Label4, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Label4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label4, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Label4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Label4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Label4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Label4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Label4, 11, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Label4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label13 = lv_label_create(ui_Panel_datetime_set);
    lv_obj_set_width(ui_Label13, 45);
    lv_obj_set_height(ui_Label13, 100);
    lv_obj_set_x(ui_Label13, 225);
    lv_obj_set_y(ui_Label13, 15);
    lv_obj_set_align(ui_Label13, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_Label13, "/");
    lv_obj_add_flag(ui_Label13, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_Label13, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label13, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Label13, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label13, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Label13, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Label13, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Label13, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Label13, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Label13, 11, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Label13, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label15 = lv_label_create(ui_Panel_datetime_set);
    lv_obj_set_width(ui_Label15, 45);
    lv_obj_set_height(ui_Label15, 100);
    lv_obj_set_x(ui_Label15, 447);
    lv_obj_set_y(ui_Label15, 15);
    lv_obj_set_align(ui_Label15, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_Label15, ":");
    lv_obj_add_flag(ui_Label15, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_color(ui_Label15, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label15, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Label15, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label15, &ui_font_hansanbold64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Label15, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Label15, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Label15, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Label15, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Label15, 11, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Label15, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_about = lv_obj_create(ui_MainMenu1);
    lv_obj_set_width(ui_Panel_about, 640);
    lv_obj_set_height(ui_Panel_about, 347);
    lv_obj_set_x(ui_Panel_about, -94);
    lv_obj_set_y(ui_Panel_about, 46);
    lv_obj_set_align(ui_Panel_about, LV_ALIGN_TOP_RIGHT);
    lv_obj_add_flag(ui_Panel_about, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(ui_Panel_about, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_about, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_about, lv_color_hex(0x282B30), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_about, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_Panel_about, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_Panel_about, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_Panel_about, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_Panel_about, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label20 = lv_label_create(ui_Panel_about);
    lv_obj_set_width(ui_Label20, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label20, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label20, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label20, "\xE5\x85\xB3\xE4\xBA\x8E\xE6\x9C\xAC\xE6\x9C\xBA");
    lv_obj_set_style_text_color(ui_Label20, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label20, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label20, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SysAbout = lv_label_create(ui_Panel_about);
    lv_obj_set_width(ui_SysAbout, 580);
    lv_obj_set_height(ui_SysAbout, 255);
    lv_obj_set_x(ui_SysAbout, 15);
    lv_obj_set_y(ui_SysAbout, 48);
    lv_label_set_text(ui_SysAbout, "\xE4\xBA\xA7\xE5\x93\x81\xE5\x90\x8D\xE7\xA7\xB0\xEF\xBC\x9A" "\n"
                                   "\xE4\xBA\xA7\xE5\x93\x81\xE5\x9E\x8B\xE5\x8F\xB7\xEF\xBC\x9A" "\n"
                                   "\xE4\xBA\xA7\xE5\x93\x81\xE5\xBA\x8F\xE5\x88\x97\xE5\x8F\xB7\xEF\xBC\x9A" "\n"
                                   "\xE8\xBD\xAF\xE4\xBB\xB6\xE7\x89\x88\xE6\x9C\xAC\xEF\xBC\x9A");
    lv_obj_add_flag(ui_SysAbout, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_add_flag(ui_SysAbout, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_SysAbout, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    lv_obj_clear_flag(ui_SysAbout, LV_OBJ_FLAG_PRESS_LOCK);      /// Flags
    lv_obj_set_style_text_color(ui_SysAbout, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_SysAbout, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_SysAbout, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_product_name = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_product_name, 380);
    lv_obj_set_height(ui_product_name, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_product_name, 164);
    lv_obj_set_y(ui_product_name, 0);
    lv_label_set_long_mode(ui_product_name, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ui_product_name, "\xE7\x91\x9E\xE8\x81\x94\xE5\x8C\xBA\xE5\x9F\x9F\xE8\xBE\x90\xE5\xB0\x84\xE7\x9B\x91\xE6\xB5\x8B\xE7\xB3\xBB\xE7\xBB\x9F"); /* 瑞联区域辐射监测系统 */
    lv_obj_set_style_text_color(ui_product_name, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_product_name, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_product_name, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_product_name, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_product_model = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_product_model, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_product_model, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_product_model, 135);
    lv_obj_set_y(ui_product_model, 36);
    lv_label_set_text(ui_product_model, "RK100P");
    lv_obj_set_style_text_color(ui_product_model, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_product_model, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_product_SN = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_product_SN, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_product_SN, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_product_SN, 164);
    lv_obj_set_y(ui_product_SN, 70);
    lv_label_set_text(ui_product_SN, "1909RWD0101");
    lv_obj_set_style_text_color(ui_product_SN, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_product_SN, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_software_version = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_software_version, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_software_version, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_software_version, 136);
    lv_obj_set_y(ui_software_version, 103);
    // \228\187\142\229\174\143\229\174\154\228\185\137\232\175\187\229\143\150\232\189\175\228\187\182\231\137\136\230\156\172
    lv_label_set_text(ui_software_version, "V1.0");
    lv_obj_set_style_text_color(ui_software_version, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_software_version, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_contact_up = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_contact_up, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_contact_up, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_contact_up, 0);
    lv_obj_set_y(ui_contact_up, 168);
    lv_label_set_text(ui_contact_up, "\xE8\x81\x94\xE7\xB3\xBB\xE6\x88\x91\xE4\xBB\xAC\xEF\xBC\x9A");
    lv_obj_set_style_text_color(ui_contact_up, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_contact_up, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_decor(ui_contact_up, LV_TEXT_DECOR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_contact_up, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_img_phone_number = lv_img_create(ui_SysAbout);
    lv_img_set_src(ui_img_phone_number, &ui_img_1640302447);
    lv_obj_set_width(ui_img_phone_number, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_img_phone_number, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_img_phone_number, 0);
    lv_obj_set_y(ui_img_phone_number, 202);
    lv_obj_add_flag(ui_img_phone_number, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_img_phone_number, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_phone_number = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_phone_number, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_phone_number, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_phone_number, 36);
    lv_obj_set_y(ui_phone_number, 202);
    lv_label_set_text(ui_phone_number, "400-8038-178");
    lv_obj_set_style_text_color(ui_phone_number, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_phone_number, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_img_address = lv_img_create(ui_SysAbout);
    lv_img_set_src(ui_img_address, &ui_img_820325126);
    lv_obj_set_width(ui_img_address, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_img_address, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_img_address, 0);
    lv_obj_set_y(ui_img_address, 278);
    lv_obj_add_flag(ui_img_address, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_img_address, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_address = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_address, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_address, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_address, 36);
    lv_obj_set_y(ui_address, 278);
    lv_label_set_text(ui_address, "\xE5\xB9\xBF\xE4\xB8\x9C\xE7\x9C\x81\xE5\xB9\xBF\xE5\xB7\x9E\xE5\xB8\x82\xE9\xBB\x84\xE5\x9F\x94\xE5\x8C\xBA" "\n"
                                  "\xE5\x8D\x97\xE7\xBF\x94\xE4\xB8\x89\xE8\xB7\xAF" "19" "\xE5\x8F\xB7" "B" "\xE5\xBA\xA7");
    lv_obj_set_style_text_color(ui_address, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_address, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_address, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_img_mailbox = lv_img_create(ui_SysAbout);
    lv_img_set_src(ui_img_mailbox, &ui_img_966723152);
    lv_obj_set_width(ui_img_mailbox, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_img_mailbox, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_img_mailbox, 0);
    lv_obj_set_y(ui_img_mailbox, 240);
    lv_obj_add_flag(ui_img_mailbox, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_img_mailbox, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_mailbox = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_mailbox, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_mailbox, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_mailbox, 36);
    lv_obj_set_y(ui_mailbox, 238);
    lv_label_set_text(ui_mailbox, "info@raydose.com");
    lv_obj_set_style_text_color(ui_mailbox, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_mailbox, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Step3.3 bind menu interaction events (UI only)
    lv_obj_add_event_cb(ui_Main_Interface, neiji_event_main, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_alarm_setting, neiji_event_alarm_setting, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_display_setting, neiji_event_display_setting, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_datetime_setting, neiji_event_datetime_setting, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_about, neiji_event_about, LV_EVENT_ALL, NULL);

    /* Alarm button group events */
    lv_obj_add_event_cb(ui_alarm_light_sw,  neiji_event_alarm_light_sw,  LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_alarm_beep_sw,   neiji_event_alarm_beep_sw,   LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Hth_Set_Btn,     neiji_event_Hth_Set_Btn,     LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Lth_Set_Btn,     neiji_event_Lth_Set_Btn,     LV_EVENT_ALL, NULL);

    /* Display button group events */
    lv_obj_add_event_cb(ui_language_sw,        neiji_event_language_sw,     LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_screen_light_sw,    neiji_event_screen_light_sw, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_LanguageChineseBtn, neiji_event_language_select, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_LanguageEnglishBtn, neiji_event_language_select, LV_EVENT_ALL, NULL);

    /* Slider events */
    lv_obj_add_event_cb(ui_Slider_volume, neiji_event_slider_volume, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Slider_bright, neiji_event_slider_bright, LV_EVENT_ALL, NULL);

    /* Threshold roller events */
    lv_obj_add_event_cb(ui_HthLeft3,  neiji_event_roller_hth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_HthLeft2,  neiji_event_roller_hth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_HthLeft1,  neiji_event_roller_hth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_HthRight1, neiji_event_roller_hth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_HthRight2, neiji_event_roller_hth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_HthUnit,   neiji_event_roller_hth, LV_EVENT_KEY, NULL);

    lv_obj_add_event_cb(ui_LthLeft3,  neiji_event_roller_lth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_LthLeft2,  neiji_event_roller_lth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_LthLeft1,  neiji_event_roller_lth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_LthRight1, neiji_event_roller_lth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_LthRight2, neiji_event_roller_lth, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_LthUnit,   neiji_event_roller_lth, LV_EVENT_KEY, NULL);

    /* Date/time roller events */
    lv_obj_add_event_cb(ui_Year,   neiji_event_roller_datetime, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_month,  neiji_event_roller_datetime, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_day,    neiji_event_roller_datetime, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_hour,   neiji_event_roller_datetime, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(ui_minute, neiji_event_roller_datetime, LV_EVENT_KEY, NULL);

    /* About scroll event */
    lv_obj_add_event_cb(ui_SysAbout, neiji_event_about_scroll, LV_EVENT_KEY, NULL);

    {
        lv_group_t *g = lv_port_indev_get_group();
        lv_group_remove_all_objs(g);
        lv_group_add_obj(g, ui_Main_Interface);
        lv_group_focus_obj(ui_Main_Interface);
    }

    lv_scr_load(ui_Main_Interface);

#if NEIJI_UI_LIVE_REFRESH
    /* 1-second refresh timer: push sensor/dose data to main UI labels */
    lv_timer_create(neiji_refresh_timer_cb, 1000, NULL);
#endif
}

void ui_Main_Interface_screen_destroy(void)
{
    if(ui_Main_Interface) lv_obj_del(ui_Main_Interface);

    // NULL screen variables
    ui_Main_Interface = NULL;
    ui_Panel_temperature = NULL;
    ui_temperature = NULL;
    ui_data_temperature = NULL;
    ui_Panel_humidity = NULL;
    ui_humidity = NULL;
    ui_data_humidity = NULL;
    ui_Panel_baro = NULL;
    ui_baro = NULL;
    ui_data_baro = NULL;
    ui_Panel_CO2 = NULL;
    ui_CO2 = NULL;
    ui_data_CO2 = NULL;
    ui_Panel_PM2_5 = NULL;
    ui_PM2_5 = NULL;
    ui_data_PM2_5 = NULL;
    ui_date = NULL;
    ui_alarm_on = NULL;
    ui_alarm_off = NULL;
    ui_led_on = NULL;
    ui_led_off = NULL;
    ui_Panel_doserate = NULL;
    ui_radiation = NULL;
    ui_data_doserate = NULL;
    ui_data_unit = NULL;
    ui_MainMenu1 = NULL;
    ui_sidebar = NULL;
    ui_alarm_setting = NULL;
    ui_display_setting = NULL;
    ui_datetime_setting = NULL;
    ui_about = NULL;
    ui_Panel_alarm_set = NULL;
    ui_Label12 = NULL;
    ui_alarm_light_sw = NULL;
    ui_Label11 = NULL;
    ui_light_alarm_state = NULL;
    ui_alarm_beep_sw = NULL;
    ui_Label21 = NULL;
    ui_alarm_volume = NULL;
    ui_Hth_Set_Btn = NULL;
    ui_Label19 = NULL;
    ui_high_threshold = NULL;
    ui_Lth_Set_Btn = NULL;
    ui_Label22 = NULL;
    ui_low_threshold = NULL;
    ui_Panel_volume_set = NULL;
    ui_set_alarm_volume = NULL;
    ui_Label8 = NULL;
    ui_Slider_volume = NULL;
    ui_Panel_high_th_set = NULL;
    ui_Label2 = NULL;
    ui_HthLeft3 = NULL;
    ui_HthLeft2 = NULL;
    ui_HthLeft1 = NULL;
    ui_HthRight1 = NULL;
    ui_HthRight2 = NULL;
    ui_HthUnit = NULL;
    ui_Label1 = NULL;
    ui_Panel_low_th_set = NULL;
    ui_Label5 = NULL;
    ui_LthLeft3 = NULL;
    ui_LthLeft2 = NULL;
    ui_LthLeft1 = NULL;
    ui_LthRight1 = NULL;
    ui_LthRight2 = NULL;
    ui_LthUnit = NULL;
    ui_Label3 = NULL;
    ui_Panel_display_set = NULL;
    ui_Label7 = NULL;
    ui_language_sw = NULL;
    ui_Label18 = NULL;
    ui_sys_language = NULL;
    ui_screen_light_sw = NULL;
    ui_Label16 = NULL;
    ui_sys_bright = NULL;
    ui_Panel_language_set = NULL;
    ui_Label17 = NULL;
    ui_LanguageChineseBtn = NULL;
    ui_labelChinese = NULL;
    ui_LanguageEnglishBtn = NULL;
    ui_labelEnglish = NULL;
    ui_Panel_bright_set = NULL;
    ui_set_bright = NULL;
    ui_Label10 = NULL;
    ui_Slider_bright = NULL;
    ui_Panel_datetime_set = NULL;
    ui_Label14 = NULL;
    ui_Year = NULL;
    ui_month = NULL;
    ui_day = NULL;
    ui_hour = NULL;
    ui_minute = NULL;
    ui_Label4 = NULL;
    ui_Label13 = NULL;
    ui_Label15 = NULL;
    ui_Panel_about = NULL;
    ui_Label20 = NULL;
    ui_SysAbout = NULL;
    ui_product_name = NULL;
    ui_product_model = NULL;
    ui_product_SN = NULL;
    ui_software_version = NULL;
    ui_contact_up = NULL;
    ui_img_phone_number = NULL;
    ui_phone_number = NULL;
    ui_img_address = NULL;
    ui_address = NULL;
    ui_img_mailbox = NULL;
    ui_mailbox = NULL;

}


/* ---- 关于本机：产品名换行 + 标签/数值列对齐 ---- */
static void neiji_adjust_about_ui_layout(void)
{
    /* ui_font_hansanbold28.line_height == 30 */
    const lv_coord_t line_h = 30;
    const int gap_rows = 1; /* 名称与型号之间空一行，三项整体略下移 */
    lv_coord_t value_x;
    lv_coord_t name_w;
    lv_coord_t name_h;
    lv_coord_t y_model;
    lv_coord_t y_sn;
    lv_coord_t y_ver;
    lv_coord_t y_contact;
    int name_rows;
    int pad_rows;
    int i;
    char left_text[160];

    if (ui_product_name == NULL || ui_product_model == NULL ||
        ui_product_SN == NULL || ui_software_version == NULL ||
        ui_SysAbout == NULL) {
        return;
    }

    if (language_get_current() == LANG_ENGLISH) {
        /* 右列统一对齐到最长标签 Software Version: 之后 */
        value_x = 250;
        name_w = 300;
    } else {
        value_x = 164;
        name_w = 380;
    }

    /* 左右同一行高，保证标签与数值竖直对齐 */
    lv_obj_set_style_text_line_space(ui_SysAbout, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_product_name, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_width(ui_product_name, name_w);
    lv_label_set_long_mode(ui_product_name, LV_LABEL_LONG_WRAP);
    lv_obj_set_x(ui_product_name, value_x);
    lv_obj_set_y(ui_product_name, 0);
    lv_obj_update_layout(ui_product_name);
    name_h = lv_obj_get_height(ui_product_name);

    name_rows = (int)((name_h + line_h - 1) / line_h);
    if (name_rows < 1) {
        name_rows = 1;
    }
    if (name_rows > 4) {
        name_rows = 4;
    }

    /* 型号/序列号/版本：名称占位后空一行再排 */
    y_model = (lv_coord_t)((name_rows + gap_rows) * line_h);
    y_sn = y_model + line_h;
    y_ver = y_sn + line_h;
    y_contact = y_ver + line_h + 10;
    /* 联系我们 → 电话 → 邮箱 → 地址，行距与初始布局一致 */
    {
        lv_coord_t y_phone = y_contact + line_h + 4;
        lv_coord_t y_mail  = y_phone + line_h + 6;
        lv_coord_t y_addr  = y_mail + line_h + 10;

        lv_obj_set_x(ui_product_model, value_x);
        lv_obj_set_y(ui_product_model, y_model);
        lv_obj_set_x(ui_product_SN, value_x);
        lv_obj_set_y(ui_product_SN, y_sn);
        lv_obj_set_x(ui_software_version, value_x);
        lv_obj_set_y(ui_software_version, y_ver);

        if (ui_contact_up) {
            lv_obj_set_y(ui_contact_up, y_contact);
        }
        if (ui_img_phone_number) {
            lv_obj_set_y(ui_img_phone_number, y_phone);
        }
        if (ui_phone_number) {
            lv_obj_set_y(ui_phone_number, y_phone);
        }
        if (ui_img_mailbox) {
            lv_obj_set_y(ui_img_mailbox, y_mail);
        }
        if (ui_mailbox) {
            lv_obj_set_y(ui_mailbox, y_mail);
        }
        if (ui_img_address) {
            lv_obj_set_y(ui_img_address, y_addr);
        }
        if (ui_address) {
            lv_obj_set_y(ui_address, y_addr);
        }
    }

    /* 左栏：名称换行空行 + 与型号之间的间隔空行，与右列 Y 对齐 */
    pad_rows = name_rows - 1 + gap_rows;
    if (pad_rows < 0) {
        pad_rows = 0;
    }

    if (language_get_current() == LANG_ENGLISH) {
        strcpy(left_text, "Product Name:");
        for (i = 0; i < pad_rows; i++) {
            strcat(left_text, "\n");
        }
        strcat(left_text, "\nProduct Model:\nSerial No:\nSoftware Version:");
    } else {
        strcpy(left_text, "\xE4\xBA\xA7\xE5\x93\x81\xE5\x90\x8D\xE7\xA7\xB0\xEF\xBC\x9A"); /* 产品名称： */
        for (i = 0; i < pad_rows; i++) {
            strcat(left_text, "\n");
        }
        strcat(left_text,
               "\n\xE4\xBA\xA7\xE5\x93\x81\xE5\x9E\x8B\xE5\x8F\xB7\xEF\xBC\x9A\n" /* 产品型号： */
               "\xE4\xBA\xA7\xE5\x93\x81\xE5\xBA\x8F\xE5\x88\x97\xE5\x8F\xB7\xEF\xBC\x9A\n" /* 产品序列号： */
               "\xE8\xBD\xAF\xE4\xBB\xB6\xE7\x89\x88\xE6\x9C\xAC\xEF\xBC\x9A"); /* 软件版本： */
    }
    lv_label_set_text(ui_SysAbout, left_text);
}

void update_all_ui_texts(void)
{
    uint32_t hi;
    uint32_t lo;
    uint32_t mask;
    uint8_t vol;

    if (!ui_Main_Interface) {
        return;
    }

    if (DeviceConfig_IsReady()) {
        DeviceConfig_GetDoseAlarmConfig(&hi, &lo, &mask, &vol);
    } else {
        mask = 0U;
    }

    /* 报警设置面板 */
    lv_label_set_text(ui_Label12, language_get_string(LANG_TITLE_ALARM_SETTING));
    lv_label_set_text(ui_Label11, language_get_string(LANG_LABEL_LED_ALARM));
    lv_label_set_text(ui_Label21, language_get_string(LANG_LABEL_BUZZER_ALARM));
    lv_label_set_text(ui_Label19, language_get_string(LANG_LABEL_HIGH_TH));
    lv_label_set_text(ui_Label22, language_get_string(LANG_LABEL_LOW_TH));
    if (ui_light_alarm_state) {
        lv_label_set_text(ui_light_alarm_state,
            sys_cfg.alarm_light ? language_get_string(LANG_STATE_ON)
                                : language_get_string(LANG_STATE_OFF));
    }

    /* 报警音量设置面板 */
    lv_label_set_text(ui_Label8, language_get_string(LANG_TITLE_VOLUME_SET));
    /* 高位阈值设置面板 */
    lv_label_set_text(ui_Label2, language_get_string(LANG_TITLE_HIGH_TH_SET));
    /* 低位阈值设置面板 */
    lv_label_set_text(ui_Label5, language_get_string(LANG_TITLE_LOW_TH_SET));

    /* 显示设置面板 */
    lv_label_set_text(ui_Label7,  language_get_string(LANG_TITLE_DISPLAY_SETTING));
    lv_label_set_text(ui_Label18, language_get_string(LANG_LABEL_SYS_LANGUAGE));
    lv_label_set_text(ui_Label16, language_get_string(LANG_LABEL_SCREEN_BRIGHT));

    /* 语言设置面板 */
    lv_label_set_text(ui_Label17,       language_get_string(LANG_TITLE_LANGUAGE_SET));
    lv_label_set_text(ui_labelChinese,  language_get_string(LANG_LABEL_CHINESE));
    lv_label_set_text(ui_labelEnglish,  language_get_string(LANG_LABEL_ENGLISH));

    /* 当前语言显示 */
    if (language_get_current() == LANG_CHINESE) {
        lv_label_set_text(ui_sys_language, language_get_string(LANG_LABEL_CHINESE));
    } else {
        lv_label_set_text(ui_sys_language, language_get_string(LANG_LABEL_ENGLISH));
    }

    /* 亮度设置面板 */
    lv_label_set_text(ui_Label10, language_get_string(LANG_TITLE_BRIGHT_SET));

    /* 日期时间设置面板 */
    lv_label_set_text(ui_Label14, language_get_string(LANG_TITLE_DATETIME_SETTING));

    /* 关于本机面板 */
    lv_label_set_text(ui_Label20,    language_get_string(LANG_TITLE_ABOUT));
    lv_label_set_text(ui_contact_up, language_get_string(LANG_LABEL_CONTACT_US));
    lv_label_set_text(ui_address,    language_get_string(LANG_VALUE_ADDRESS));
    neiji_bind_about_info(); /* 内含产品名换行布局与左栏标签对齐 */
    Ui_AlarmStatus_Refresh();
}

void ui_Main_Interface_screen_relocalize(void)
{
    update_all_ui_texts();
}
