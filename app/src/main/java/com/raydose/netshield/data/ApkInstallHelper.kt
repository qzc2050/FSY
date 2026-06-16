package com.raydose.netshield.data

import android.app.Activity
import android.content.ClipData
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.content.pm.PackageInstaller
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log
import androidx.core.content.FileProvider
import java.io.File

/** 从本地/U 盘选择 APK 后，调起系统安装界面完成本机应用更新。 */
object ApkInstallHelper {

    private const val TAG = "NetShield"
    private val knownInstallerPackages = listOf(
        "com.android.packageinstaller",
        "com.google.android.packageinstaller",
        "com.android.permissioncontroller",
    )

    fun canInstallPackages(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return true
        return context.packageManager.canRequestPackageInstalls()
    }

    fun unknownSourcesSettingsIntent(context: Context): Intent =
        Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES).apply {
            data = Uri.parse("package:${context.packageName}")
        }

    fun installApk(context: Context, apkFile: File): Result<Unit> = runCatching {
        require(apkFile.isFile && apkFile.exists()) { "安装包不存在" }
        require(apkFile.length() > 0L) { "安装包为空，请重新选择" }

        val sessionError = runCatching { installWithPackageInstaller(context, apkFile) }
        if (sessionError.isSuccess) return@runCatching

        Log.w(TAG, "PackageInstaller failed, fallback to VIEW intent", sessionError.exceptionOrNull())
        installWithViewIntent(context, apkFile)
    }

    private fun installWithPackageInstaller(context: Context, apkFile: File) {
        val installer = context.packageManager.packageInstaller
        val params = PackageInstaller.SessionParams(
            PackageInstaller.SessionParams.MODE_FULL_INSTALL,
        ).apply {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                setInstallReason(PackageManager.INSTALL_REASON_USER)
            }
        }
        val sessionId = installer.createSession(params)
        val session = installer.openSession(sessionId)
        try {
            apkFile.inputStream().buffered().use { input ->
                session.openWrite("base.apk", 0, apkFile.length()).use { output ->
                    input.copyTo(output)
                    session.fsync(output)
                }
            }
            val callbackIntent = Intent(context.applicationContext, ApkInstallResultReceiver::class.java).apply {
                action = ApkInstallResultReceiver.ACTION
            }
            val pendingIntent = android.app.PendingIntent.getBroadcast(
                context.applicationContext,
                sessionId,
                callbackIntent,
                pendingIntentFlags(),
            )
            session.commit(pendingIntent.intentSender)
        } catch (error: Exception) {
            session.abandon()
            throw error
        } finally {
            session.close()
        }
    }

    private fun installWithViewIntent(context: Context, apkFile: File) {
        val appContext = context.applicationContext
        val uri = FileProvider.getUriForFile(
            appContext,
            "${appContext.packageName}.fileprovider",
            apkFile,
        )
        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "application/vnd.android.package-archive")
            addFlags(
                Intent.FLAG_GRANT_READ_URI_PERMISSION or
                    Intent.FLAG_ACTIVITY_NEW_TASK,
            )
            clipData = ClipData.newRawUri("", uri)
        }
        val handlers = appContext.packageManager.queryIntentActivities(
            intent,
            PackageManager.MATCH_DEFAULT_ONLY,
        )
        require(handlers.isNotEmpty()) { "未找到系统安装程序" }
        val targetPackages = handlers.map { it.activityInfo.packageName }.toSet() + knownInstallerPackages
        targetPackages.forEach { packageName ->
            runCatching {
                appContext.grantUriPermission(
                    packageName,
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION,
                )
            }
        }
        val launchContext = context.findActivity() ?: appContext
        launchContext.startActivity(intent)
    }

    private fun pendingIntentFlags(): Int {
        val base = android.app.PendingIntent.FLAG_UPDATE_CURRENT
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            base or android.app.PendingIntent.FLAG_MUTABLE
        } else {
            base
        }
    }

    private fun Context.findActivity(): Activity? {
        var current: Context = this
        while (current is ContextWrapper) {
            if (current is Activity) return current
            current = current.baseContext
        }
        return null
    }
}
