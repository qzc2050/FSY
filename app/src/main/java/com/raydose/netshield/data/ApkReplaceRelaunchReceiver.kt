package com.raydose.netshield.data

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log

/**
 * 覆盖安装完成后软拉起本应用（不重启整机）。
 */
class ApkReplaceRelaunchReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_MY_PACKAGE_REPLACED) return
        val pending = goAsync()
        try {
            Log.i(TAG, "MY_PACKAGE_REPLACED: soft relaunch app")
            ApkRelaunchHelper.relaunchAfterUpdate(context)
        } finally {
            pending.finish()
        }
    }

    companion object {
        private const val TAG = "NetShield"
    }
}
