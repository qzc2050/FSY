package com.raydose.raylink

import android.app.LocaleManager
import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.LocaleList
import com.raydose.raylink.model.AppLanguage
import java.util.Locale

/**
 * 应用内语言：复用 [DisplaySoundSettings.language]。
 * API 33+ 用 [LocaleManager]；更低版本用 [wrap] + Activity.recreate。
 */
object AppLocale {
    fun locale(language: AppLanguage): Locale = when (language) {
        AppLanguage.Zh -> Locale.SIMPLIFIED_CHINESE
        AppLanguage.En -> Locale.ENGLISH
    }

    fun wrap(base: Context, language: AppLanguage): Context {
        val loc = locale(language)
        Locale.setDefault(loc)
        val config = Configuration(base.resources.configuration)
        config.setLocales(LocaleList(loc))
        return base.createConfigurationContext(config)
    }

    fun apply(context: Context, language: AppLanguage) {
        val loc = locale(language)
        Locale.setDefault(loc)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            val tags = when (language) {
                AppLanguage.Zh -> "zh-CN"
                AppLanguage.En -> "en"
            }
            val manager = context.getSystemService(LocaleManager::class.java)
            manager?.applicationLocales = LocaleList.forLanguageTags(tags)
        }
    }
}
