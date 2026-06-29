/**
 * @file language.h
 * @brief 多语言支持模块 - 中英文切换 (NeiJi 适配版)
 */

#ifndef _LANGUAGE_H_
#define _LANGUAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LANG_CHINESE = 0,
    LANG_ENGLISH = 1,
    LANG_MAX
} Language_t;

typedef struct {
    const char *zh;
    const char *en;
} Lang_String_t;

/* 所有可本地化文本的 ID */
typedef enum {
    /* ---- 菜单/面板标题 ---- */
    LANG_TITLE_ALARM_SETTING = 0,
    LANG_TITLE_DISPLAY_SETTING,
    LANG_TITLE_DATETIME_SETTING,
    LANG_TITLE_ABOUT,
    LANG_TITLE_VOLUME_SET,
    LANG_TITLE_HIGH_TH_SET,
    LANG_TITLE_LOW_TH_SET,
    LANG_TITLE_BRIGHT_SET,
    LANG_TITLE_LANGUAGE_SET,

    /* ---- 报警设置面板 ---- */
    LANG_LABEL_LED_ALARM,
    LANG_LABEL_BUZZER_ALARM,
    LANG_LABEL_HIGH_TH,
    LANG_LABEL_LOW_TH,
    LANG_STATE_ON,
    LANG_STATE_OFF,

    /* ---- 显示设置面板 ---- */
    LANG_LABEL_SYS_LANGUAGE,
    LANG_LABEL_SCREEN_BRIGHT,

    /* ---- 语言面板 ---- */
    LANG_LABEL_CHINESE,
    LANG_LABEL_ENGLISH,

    /* ---- 关于面板 ---- */
    LANG_LABEL_PRODUCT_INFO,       /* 产品名称：\n产品型号：\n...
                                      多行标签 */
    LANG_LABEL_CONTACT_US,
    LANG_VALUE_PRODUCT_NAME,
    LANG_VALUE_ADDRESS,

    LANG_MAX_COUNT
} Lang_Text_ID_t;

Language_t   language_get_current(void);
void         language_set_current(Language_t lang);
const char * language_get_string(Lang_Text_ID_t id);
void         language_init(void);
void         language_refresh_ui_texts(void);

/* 由 ui_Main_Interface.c 提供，实际刷新所有 UI 文本 */
void update_all_ui_texts(void);

#ifdef __cplusplus
}
#endif

#endif /* _LANGUAGE_H_ */
