// This file was generated from RAD-I SquareLine layout for NeiJi static UI migration
// Business events, timers and hardware dependencies are intentionally not wired in Step3.2.

#include "ui_Main_Interface.h"

#include "../ui.h"

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
    lv_obj_set_style_text_font(ui_Main_Interface, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel_temperature = lv_obj_create(ui_Main_Interface);
    lv_obj_set_width(ui_Panel_temperature, 140);
    lv_obj_set_height(ui_Panel_temperature, 155);
    lv_obj_set_x(ui_Panel_temperature, 6);
    lv_obj_set_y(ui_Panel_temperature, 0);
    lv_obj_set_align(ui_Panel_temperature, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_clear_flag(ui_Panel_temperature, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel_temperature, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel_temperature, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_temperature, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_style_bg_color(ui_Panel_humidity, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_humidity, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_style_bg_color(ui_Panel_baro, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_baro, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_label_set_text(ui_data_baro, "1013 hPa");
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
    lv_obj_set_style_bg_color(ui_Panel_CO2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_CO2, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_label_set_text(ui_data_CO2, "400 ppm");
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
    lv_obj_set_style_bg_color(ui_Panel_PM2_5, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_PM2_5, 60, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_style_bg_color(ui_Panel_doserate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel_doserate, 60, LV_PART_MAIN | LV_STATE_DEFAULT);

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
    lv_label_set_text(ui_Label12, "\230\138\165\232\173\166\232\174\190\231\189\174");
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
    lv_label_set_text(ui_Label11, "\231\129\175\229\133\137\230\138\165\232\173\166\239\188\154");
    lv_obj_set_style_text_color(ui_Label11, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label11, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label11, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_light_alarm_state = lv_label_create(ui_alarm_light_sw);
    lv_obj_set_width(ui_light_alarm_state, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_light_alarm_state, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_light_alarm_state, 110);
    lv_obj_set_y(ui_light_alarm_state, 0);
    lv_obj_set_align(ui_light_alarm_state, LV_ALIGN_CENTER);
    lv_label_set_text(ui_light_alarm_state, "\229\188\128");
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
    lv_label_set_text(ui_Label21, "\232\156\130\233\184\163\229\153\168\230\138\165\232\173\166\239\188\154");
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
    lv_label_set_text(ui_Label19, "\233\171\152\228\189\141\233\152\136\229\128\188\239\188\154");
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
    lv_label_set_text(ui_Label22, "\228\189\142\228\189\141\233\152\136\229\128\188\239\188\154");
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
    lv_label_set_text(ui_Label8, "\230\138\165\232\173\166\233\159\179\233\135\143\232\174\190\231\189\174");
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
    lv_label_set_text(ui_Label2, "\233\171\152\228\189\141\233\152\136\229\128\188\232\174\190\231\189\174");
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
    lv_label_set_text(ui_Label5, "\228\189\142\228\189\141\233\152\136\229\128\188\232\174\190\231\189\174");
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
    lv_label_set_text(ui_Label7, "\230\152\190\231\164\186\232\174\190\231\189\174");
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
    lv_label_set_text(ui_Label18, "\231\179\187\231\187\159\232\175\173\232\168\128\239\188\154");
    lv_obj_set_style_text_color(ui_Label18, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label18, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label18, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_sys_language = lv_label_create(ui_language_sw);
    lv_obj_set_width(ui_sys_language, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_sys_language, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_sys_language, 169);
    lv_obj_set_y(ui_sys_language, 101);
    lv_label_set_text(ui_sys_language, "\228\184\173\230\150\135");
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
    lv_label_set_text(ui_Label16, "\229\177\143\229\185\149\228\186\174\229\186\166\239\188\154");
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
    lv_label_set_text(ui_Label17, "\231\179\187\231\187\159\232\175\173\232\168\128\232\174\190\231\189\174");
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
    lv_label_set_text(ui_labelChinese, "\228\184\173\230\150\135");
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
    lv_label_set_text(ui_Label10, "\229\177\143\229\185\149\228\186\174\229\186\166\232\174\190\231\189\174");
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
    lv_label_set_text(ui_Label14, "\230\151\165\230\156\159\229\146\140\230\151\182\233\151\180\232\174\190\231\189\174");
    lv_obj_set_style_text_color(ui_Label14, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label14, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label14, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Year = lv_roller_create(ui_Panel_datetime_set);
    lv_roller_set_options(ui_Year,
                            "99\n98\n97\n96\n95\n94\n93\n92\n91\n90\n89\n88\n87\n86\n85\n84\n83\n82\n81\n80\n79\n78\n77\n76\n75\n74\n73\n72\n71\n70\n69\n68\n67\n66\n65\n64\n63\n62\n61\n60\n59\n58\n57\n56\n55\n54\n53\n52\n51\n50\n49\n48\n47\n46\n45\n44\n43\n42\n41\n40\n39\n38\n37\n36\n35\n34\n33\n32\n31\n30\n29\n28\n27\n26\n25\n24\n23\n22\n21\n20\n19\n18\n17\n16\n15\n14\n13\n12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01\n00",
                            LV_ROLLER_MODE_INFINITE);
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
    lv_roller_set_options(ui_month, "12\n11\n10\n09\n08\n07\n06\n05\n04\n03\n02\n01", LV_ROLLER_MODE_INFINITE);
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
                            LV_ROLLER_MODE_INFINITE);
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
                            LV_ROLLER_MODE_INFINITE);
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
                            LV_ROLLER_MODE_INFINITE);
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
    lv_label_set_text(ui_Label20, "\229\133\179\228\186\142\230\156\172\230\156\186");
    lv_obj_set_style_text_color(ui_Label20, lv_color_hex(0xD2D2D2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label20, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Label20, &ui_font_hansanbold32, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SysAbout = lv_label_create(ui_Panel_about);
    lv_obj_set_width(ui_SysAbout, 580);
    lv_obj_set_height(ui_SysAbout, 255);
    lv_obj_set_x(ui_SysAbout, 15);
    lv_obj_set_y(ui_SysAbout, 48);
    lv_label_set_text(ui_SysAbout, "\228\186\167\229\147\129\229\144\141\231\167\176\239\188\154\n\228\186\167\229\147\129\229\158\139\229\143\183\239\188\154\n\228\186\167\229\147\129\229\186\143\229\136\151\229\143\183\239\188\154\n\232\189\175\228\187\182\231\137\136\230\156\172\239\188\154");
    lv_obj_add_flag(ui_SysAbout, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_clear_flag(ui_SysAbout, LV_OBJ_FLAG_PRESS_LOCK);      /// Flags
    lv_obj_set_style_text_color(ui_SysAbout, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_SysAbout, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_SysAbout, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_product_name = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_product_name, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_product_name, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_product_name, 135);
    lv_obj_set_y(ui_product_name, 0);
    lv_label_set_text(ui_product_name, "\233\155\183\230\178\131 \230\142\162\230\181\139\228\187\142\230\156\186");
    lv_obj_set_style_text_color(ui_product_name, lv_color_hex(0xC6E1FF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_product_name, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_product_name, &ui_font_hansanbold28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_product_model = lv_label_create(ui_SysAbout);
    lv_obj_set_width(ui_product_model, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_product_model, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_product_model, 135);
    lv_obj_set_y(ui_product_model, 36);
    lv_label_set_text(ui_product_model, "RWD-I");
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
    lv_label_set_text(ui_contact_up, "\232\129\148\231\179\187\230\136\145\228\187\172\239\188\154");
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
    lv_label_set_text(ui_address, "\229\185\191\228\184\156\231\156\129\229\185\191\229\183\158\229\184\130\233\187\132\229\159\148\229\140\186\n\229\141\151\231\191\148\228\184\137\232\183\17519\229\143\183B\229\186\167");
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

    lv_scr_load(ui_Main_Interface);
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


void ui_Main_Interface_screen_relocalize(void)
{
}
