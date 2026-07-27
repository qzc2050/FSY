package com.raydose.raylink

import android.app.Application
import com.raydose.raylink.data.HostSettingsRepository

class RaylinkApp : Application() {
    override fun onCreate() {
        super.onCreate()
        val language = HostSettingsRepository(this).loadDisplaySound().language
        AppLocale.apply(this, language)
    }
}
