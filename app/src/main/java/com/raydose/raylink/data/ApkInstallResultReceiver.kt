package com.raydose.raylink.data

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.os.Build
import android.util.Log
import android.widget.Toast

/** PackageInstaller 安装结果回调（系统安装界面由 session.commit 调起）。 */
class ApkInstallResultReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        val pending = goAsync()
        try {
            val status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE)
            val message = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE).orEmpty()
            when (status) {
                PackageInstaller.STATUS_SUCCESS -> {
                    Log.i(TAG, "APK install session succeeded; relaunch app")
                    ApkRelaunchHelper.relaunchAfterUpdate(context)
                }
                PackageInstaller.STATUS_PENDING_USER_ACTION -> {
                    val confirm = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        intent.getParcelableExtra(Intent.EXTRA_INTENT, Intent::class.java)
                    } else {
                        @Suppress("DEPRECATION")
                        intent.getParcelableExtra(Intent.EXTRA_INTENT)
                    }
                    if (confirm != null) {
                        confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_NO_ANIMATION)
                        context.startActivity(confirm)
                    } else {
                        Log.w(TAG, "Install pending user action but no confirm intent")
                    }
                }
                else -> {
                    Log.w(TAG, "APK install session failed: status=$status msg=$message")
                    ApkRelaunchHelper.cancelScheduledRelaunch(context)
                    ApkUpdateBrightnessCover.restoreIfNeeded(context)
                    showInstallFailureToast(context.applicationContext, message)
                }
            }
        } finally {
            pending.finish()
        }
    }

    companion object {
        private const val TAG = "Raylink"
        const val ACTION = "com.raydose.raylink.APK_INSTALL_RESULT"

        private fun showInstallFailureToast(context: Context, rawMessage: String) {
            val text = when {
                rawMessage.contains("VERSION_DOWNGRADE", ignoreCase = true) ->
                    "安装失败：所选 APK 版本低于当前已安装版本，请选择更高版本的安装包"
                rawMessage.contains("SIGNATURE", ignoreCase = true) ->
                    "安装失败：签名不一致，无法覆盖安装"
                rawMessage.isNotBlank() -> "安装失败：$rawMessage"
                else -> "安装失败，请重试"
            }
            Toast.makeText(context, text, Toast.LENGTH_LONG).show()
        }
    }
}
