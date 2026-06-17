/**
 * @file language.c
 * @brief 多语言支持模块 - 中英文切换实现
 * @version 1.0
 * @date 2026-05-12
 */

#include "language.h"
#include "ui.h"
#include "geiger.h"
#include <string.h>

/**
 * @brief 当前语言（默认为中文）
 */
static Language_t current_language = LANG_CHINESE;

/**
 * @brief 所有语言字符串表
 */
static const Lang_String_t lang_strings[LANG_MAX_COUNT] = {
    // 主界面 - 传感器标签
    [LANG_LABEL_TEMPERATURE]     = {"温度", "Temperature"},
    [LANG_LABEL_HUMIDITY]        = {"湿度", "Humidity"},
    [LANG_LABEL_BARO]            = {"大气压力", "Pressure"},
    [LANG_LABEL_CO2]             = {"二氧化碳", "CO2"},
    [LANG_LABEL_PM25]            = {"PM2.5", "PM2.5"},
    [LANG_LABEL_RADIATION]       = {"辐射", "Radiation"},
    
    // 菜单项
    [LANG_MENU_ALARM_SETTING]    = {"报警设置", "Alarm Settings"},
    [LANG_MENU_DISPLAY_SETTING]  = {"显示设置", "Display Settings"},
    [LANG_MENU_DATETIME_SETTING] = {"日期时间", "Date & Time"},
    [LANG_MENU_ABOUT]            = {"关于本机", "About"},
    
    // 报警设置
    [LANG_ALARM_LIGHT_LABEL]     = {"灯光报警：", "LED Alarm:"},
    [LANG_ALARM_BUZZER_LABEL]    = {"蜂鸣器报警：", "Buzzer Alarm:"},
    [LANG_ALARM_HIGH_THRESHOLD]  = {"高位阈值：", "High Threshold:"},
    [LANG_ALARM_LOW_THRESHOLD]   = {"低位阈值：", "Low Threshold:"},
    [LANG_ALARM_STATE_ON]        = {"开", "ON"},
    [LANG_ALARM_STATE_OFF]       = {"关", "OFF"},
    [LANG_ALARM_VOLUME_LABEL]    = {"报警音量：", "Alarm Volume:"},
    
    // 报警设置面板标题
    [LANG_ALARM_VOLUME_SET_TITLE]   = {"报警音量设置", "Alarm Volume Setting"},
    [LANG_HIGH_THRESHOLD_SET_TITLE] = {"高位阈值设置", "High Threshold Setting"},
    [LANG_LOW_THRESHOLD_SET_TITLE]  = {"低位阈值设置", "Low Threshold Setting"},
    
    // 显示设置
    [LANG_DISPLAY_SETTING_TITLE]     = {"显示设置", "Display Settings"},
    [LANG_SYSTEM_LANGUAGE_LABEL]     = {"系统语言：", "Language:"},
    [LANG_SCREEN_BRIGHTNESS_LABEL]   = {"屏幕亮度：", "Brightness:"},
    
    // 语言设置
    [LANG_LANGUAGE_SET_TITLE]     = {"系统语言设置", "Language Settings"},
    [LANG_LANGUAGE_CHINESE]       = {"中文", "Chinese"},
    [LANG_LANGUAGE_ENGLISH]       = {"English", "English"},
    
    // 亮度设置
    [LANG_BRIGHTNESS_SET_TITLE]   = {"屏幕亮度设置", "Brightness Setting"},
    
    // 日期时间设置
    [LANG_DATETIME_SET_TITLE]     = {"日期和时间设置", "Date & Time Settings"},
    
    // 关于本机
    [LANG_ABOUT_TITLE]              = {"关于本机", "About"},
    [LANG_ABOUT_PRODUCT_NAME]       = {"产品名称：\n产品型号：\n产品序列号：\n软件版本：", 
                                       "Product Name:\nProduct Model:\nSN:\nSoftware Version:"},
    [LANG_ABOUT_PRODUCT_MODEL]      = {"产品型号：", "Product Model:"},
    [LANG_ABOUT_PRODUCT_SN]         = {"产品序列号：", "SN:"},
    [LANG_ABOUT_SOFTWARE_VERSION]   = {"软件版本：", "Software Version:"},
    [LANG_ABOUT_CONTACT_US]         = {"联系我们：", "Contact Us:"},
    [LANG_ABOUT_PHONE]              = {"电话：", "Phone:"},
    [LANG_ABOUT_ADDRESS]            = {"地址：", "Address:"},
    [LANG_ABOUT_EMAIL]              = {"邮箱：", "Email:"},
    
    // 单位
    [LANG_UNIT_TEMPERATURE]     = {"℃", "°C"},
    [LANG_UNIT_HUMIDITY]        = {"%", "%"},
    [LANG_UNIT_BARO]            = {"hPa", "hPa"},
    [LANG_UNIT_CO2]             = {"ppm", "ppm"},
    [LANG_UNIT_PM25]            = {"ug/m³", "μg/m³"},
    [LANG_UNIT_DOSE_RATE]       = {"μSv/h", "μSv/h"},
    
    // 产品具体信息
    [LANG_PRODUCT_NAME_VALUE]       = {"雷沃 探测从机", "RayWatch Detector"},
    [LANG_PRODUCT_MODEL_VALUE]      = {"RWD-I", "RWD-I"},
    [LANG_PRODUCT_SN_VALUE]         = {"1909RWD0101", "1909RWD0101"},
    [LANG_SOFTWARE_VERSION_VALUE]   = {"V1.0", "V1.0"},
    [LANG_CONTACT_PHONE_VALUE]      = {"020-400 8038 178", "020-400 8038 178"},
    [LANG_CONTACT_ADDRESS_VALUE]    = {"中国广州市黄埔区\n南翔三路 19 号 B 座", 
                                        " Block B, No.19 Nanxiang 3rd Road, Huangpu District,\nGuangzhou, China"},
    [LANG_CONTACT_EMAIL_VALUE]      = {"info@raydose.com", "info@raydose.com"},
    
    // 其他符号
    [LANG_SYMBOL_SEPARATOR]     = {"/", "/"},
    [LANG_SYMBOL_COLON]         = {":", ":"},
    [LANG_SYMBOL_DOT]           = {".", "."},
};

/**
 * @brief 获取当前语言
 */
Language_t language_get_current(void)
{
    return current_language;
}

/**
 * @brief 设置当前语言
 */
void language_set_current(Language_t lang)
{
    if(lang < LANG_MAX) {
        current_language = lang;
    }
}

/**
 * @brief 获取指定 ID 的语言字符串
 */
const char* language_get_string(Lang_Text_ID_t id)
{
    if(id < LANG_MAX_COUNT) {
        if(current_language == LANG_CHINESE) {
            return lang_strings[id].zh;
        } else {
            return lang_strings[id].en;
        }
    }
    return "";
}

/**
 * @brief 初始化语言模块
 */
void language_init(void)
{
    // 从系统配置中读取已保存的语言设置
    current_language = (Language_t)sys_cfg.language;
    
    // 如果值无效，默认为中文
    if(current_language >= LANG_MAX) {
        current_language = LANG_CHINESE;
        sys_cfg.language = LANG_CHINESE;
    }
}

/**
 * @brief 刷新所有界面文本
 * 
 * @note 此函数需要根据实际 UI 对象进行实现
 *       由于 UI 对象定义在 ui_Main_Interface.c 中
 *       需要在此处引用这些对象并更新其文本
 */
void language_refresh_ui_texts(void)
{
    // 调用 ui_Main_Interface.c 中的实际更新函数
    update_all_ui_texts();
}
