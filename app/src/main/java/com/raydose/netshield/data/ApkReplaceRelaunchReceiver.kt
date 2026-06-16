package com.raydose.netshield.data

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/** 覆盖安装完成后自动回到本应用（避免停在系统桌面）。 */
class ApkReplaceRelaunchReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_MY_PACKAGE_REPLACED) return
        ApkRelaunchHelper.relaunchAfterUpdate(context)
    }
}
