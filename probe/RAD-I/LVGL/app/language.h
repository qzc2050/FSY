/**
 * @file language.h
 * @brief 多语言支持模块 - 中英文切换
 * @version 1.0
 * @date 2026-05-12
 */

#ifndef _LANGUAGE_H_
#define _LANGUAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 语言类型枚举
 */
typedef enum {
    LANG_CHINESE = 0,     // 中文
    LANG_ENGLISH = 1,     // 英文
    LANG_MAX              // 语言数量
} Language_t;

/**
 * @brief 语言字符串结构体
 */
typedef struct {
    const char *zh;       // 中文字符串
    const char *en;       // 英文字符串
} Lang_String_t;

/**
 * @brief 语言文本 ID 枚举（用于索引所有需要翻译的文本）
 */
typedef enum {
    // 主界面 - 传感器标签
    LANG_LABEL_TEMPERATURE = 0,
    LANG_LABEL_HUMIDITY,
    LANG_LABEL_BARO,
    LANG_LABEL_CO2,
    LANG_LABEL_PM25,
    LANG_LABEL_RADIATION,
    
    // 菜单项
    LANG_MENU_ALARM_SETTING,
    LANG_MENU_DISPLAY_SETTING,
    LANG_MENU_DATETIME_SETTING,
    LANG_MENU_ABOUT,
    
    // 报警设置
    LANG_ALARM_LIGHT_LABEL,
    LANG_ALARM_BUZZER_LABEL,
    LANG_ALARM_HIGH_THRESHOLD,
    LANG_ALARM_LOW_THRESHOLD,
    LANG_ALARM_STATE_ON,
    LANG_ALARM_STATE_OFF,
    LANG_ALARM_VOLUME_LABEL,
    
    // 报警设置面板标题
    LANG_ALARM_VOLUME_SET_TITLE,
    LANG_HIGH_THRESHOLD_SET_TITLE,
    LANG_LOW_THRESHOLD_SET_TITLE,
    
    // 显示设置
    LANG_DISPLAY_SETTING_TITLE,
    LANG_SYSTEM_LANGUAGE_LABEL,
    LANG_SCREEN_BRIGHTNESS_LABEL,
    
    // 语言设置
    LANG_LANGUAGE_SET_TITLE,
    LANG_LANGUAGE_CHINESE,
    LANG_LANGUAGE_ENGLISH,
    
    // 亮度设置
    LANG_BRIGHTNESS_SET_TITLE,
    
    // 日期时间设置
    LANG_DATETIME_SET_TITLE,
    
    // 关于本机
    LANG_ABOUT_TITLE,
    LANG_ABOUT_PRODUCT_NAME,
    LANG_ABOUT_PRODUCT_MODEL,
    LANG_ABOUT_PRODUCT_SN,
    LANG_ABOUT_SOFTWARE_VERSION,
    LANG_ABOUT_CONTACT_US,
    LANG_ABOUT_PHONE,
    LANG_ABOUT_ADDRESS,
    LANG_ABOUT_EMAIL,
    
    // 单位
    LANG_UNIT_TEMPERATURE,
    LANG_UNIT_HUMIDITY,
    LANG_UNIT_BARO,
    LANG_UNIT_CO2,
    LANG_UNIT_PM25,
    LANG_UNIT_DOSE_RATE,
    
    // 产品具体信息
    LANG_PRODUCT_NAME_VALUE,
    LANG_PRODUCT_MODEL_VALUE,
    LANG_PRODUCT_SN_VALUE,
    LANG_SOFTWARE_VERSION_VALUE,
    LANG_CONTACT_PHONE_VALUE,
    LANG_CONTACT_ADDRESS_VALUE,
    LANG_CONTACT_EMAIL_VALUE,
    
    // 其他符号
    LANG_SYMBOL_SEPARATOR,
    LANG_SYMBOL_COLON,
    LANG_SYMBOL_DOT,
    
    // 语言总数
    LANG_MAX_COUNT
} Lang_Text_ID_t;

/**
 * @brief 获取当前语言
 * @return 当前语言类型
 */
Language_t language_get_current(void);

/**
 * @brief 设置当前语言
 * @param lang 要设置的语言类型
 */
void language_set_current(Language_t lang);

/**
 * @brief 获取指定 ID 的语言字符串
 * @param id 语言文本 ID
 * @return 对应语言的字符串
 */
const char* language_get_string(Lang_Text_ID_t id);

/**
 * @brief 初始化语言模块
 */
void language_init(void);

/**
 * @brief 刷新所有界面文本（语言切换后调用）
 */
void language_refresh_ui_texts(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* _LANGUAGE_H_ */
