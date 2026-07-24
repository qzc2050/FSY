/**
 * @file language.c
 * @brief 多语言支持模块 - NeiJi 适配版
 */

#include "language.h"
#include "geiger.h"
#include "sys_cfg_defaults.h"

static Language_t current_language = LANG_CHINESE;

static const Lang_String_t lang_strings[LANG_MAX_COUNT] = {
    [LANG_TITLE_ALARM_SETTING]     = {"报警设置",           "Alarm Settings"},
    [LANG_TITLE_DISPLAY_SETTING]   = {"显示设置",           "Display Settings"},
    [LANG_TITLE_DATETIME_SETTING]  = {"日期和时间设置",     "Date & Time Settings"},
    [LANG_TITLE_ABOUT]             = {"关于本机",           "About"},
    [LANG_TITLE_VOLUME_SET]        = {"报警音量设置",       "Alarm Volume Setting"},
    [LANG_TITLE_HIGH_TH_SET]       = {"高位阈值设置",       "High Threshold Setting"},
    [LANG_TITLE_LOW_TH_SET]        = {"低位阈值设置",       "Low Threshold Setting"},
    [LANG_TITLE_BRIGHT_SET]        = {"屏幕亮度设置",       "Brightness Setting"},
    [LANG_TITLE_LANGUAGE_SET]      = {"系统语言设置",       "Language Setting"},

    [LANG_LABEL_LED_ALARM]         = {"灯光报警：",         "LED Alarm:"},
    [LANG_LABEL_BUZZER_ALARM]      = {"蜂鸣器报警：",       "Buzzer Alarm:"},
    [LANG_LABEL_HIGH_TH]           = {"高位阈值：",         "High Threshold:"},
    [LANG_LABEL_LOW_TH]            = {"低位阈值：",         "Low Threshold:"},
    [LANG_STATE_ON]                = {"开",                 "ON"},
    [LANG_STATE_OFF]               = {"关",                 "OFF"},

    [LANG_LABEL_SYS_LANGUAGE]      = {"系统语言：",         "Language:"},
    [LANG_LABEL_SCREEN_BRIGHT]     = {"屏幕亮度：",         "Brightness:"},

    [LANG_LABEL_CHINESE]           = {"中文",               "Chinese"},
    [LANG_LABEL_ENGLISH]           = {"English",            "English"},

    [LANG_LABEL_PRODUCT_INFO]      = {"产品名称：\n产品型号：\n产品序列号：\n软件版本：",
                                       "Product Name:\nProduct Model:\nSerial No:\nSoftware Version:"},
    [LANG_LABEL_CONTACT_US]        = {"联系我们：",         "Contact Us:"},
    [LANG_VALUE_PRODUCT_NAME]      = {"瑞联区域辐射监测系统",
                                       "Raylink Area Radiation Monitoring System"},
    [LANG_VALUE_ADDRESS]           = {"广东省广州市黄埔区\n南翔三路19号B座",
                                       "Block B, No.19 Nanxiang 3rd Road\nHuangpu District, Guangzhou, China"},
};

Language_t language_get_current(void)
{
    return current_language;
}

void language_set_current(Language_t lang)
{
    if (lang < LANG_MAX) {
        current_language = lang;
    }
}

const char * language_get_string(Lang_Text_ID_t id)
{
    if (id < LANG_MAX_COUNT) {
        if (current_language == LANG_CHINESE)
            return lang_strings[id].zh;
        else
            return lang_strings[id].en;
    }
    return "";
}

void language_init(void)
{
    current_language = (Language_t)sys_cfg.language;
    if (current_language >= LANG_MAX) {
        current_language = LANG_CHINESE;
        sys_cfg.language = LANG_CHINESE;
    }
}

void language_refresh_ui_texts(void)
{
    update_all_ui_texts();
}
