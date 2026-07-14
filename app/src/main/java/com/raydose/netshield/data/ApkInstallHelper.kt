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
import android.util.Base64
import android.util.Log
import androidx.core.content.FileProvider
import java.io.File
import java.nio.charset.StandardCharsets

/** 从本地/U 盘选择 APK 后，完成本机应用更新。优先 root 静默安装，成功后仅重启本应用。 */
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

        // 覆盖安装会杀进程：无论 root / 系统安装器，都先预埋拉回闹钟
        // 灭屏改到 stage/写入会话完成、真正 install/commit 之前，避免 cp 期间已黑屏
        ApkRelaunchHelper.armBeforeSelfUpdate(context)

        try {
            if (tryInstallWithRootSilent(context, apkFile)) {
                Log.i(TAG, "APK self-update via root pm install started; app will relaunch")
                ApkRelaunchHelper.relaunchAfterUpdate(context)
                return@runCatching
            }

            val sessionError = runCatching { installWithPackageInstaller(context, apkFile) }
            if (sessionError.isSuccess) return@runCatching

            Log.w(TAG, "PackageInstaller failed, fallback to VIEW intent", sessionError.exceptionOrNull())
            installWithViewIntent(context, apkFile)
        } catch (error: Exception) {
            ApkUpdateBrightnessCover.restoreIfNeeded(context)
            throw error
        }
    }

    /**
     * 工控机有 root 时：setsid 执行 pm install 静默覆盖。
     * 顺序：cp 到 /data/local/tmp → dimBeforeUpdate → setsid install.sh（含 pm install）。
     * 不重启整机；由 AlarmManager / MY_PACKAGE_REPLACED 拉回本应用。
     */
    private fun tryInstallWithRootSilent(context: Context, apkFile: File): Boolean {
        if (!RootShell.isAvailable()) return false
        val tmpApk = "/data/local/tmp/netshield_update.apk"
        val tmpLog = "/data/local/tmp/netshield_update.log"
        val installSh = "/data/local/tmp/netshield_install.sh"
        val src = RootShell.quote(apkFile.absolutePath)

        val stage = RootShell.run(
            "cp $src $tmpApk && chmod 644 $tmpApk && sync && stat -c %s $tmpApk",
        )
        if (!stage.isSuccess) {
            Log.w(
                TAG,
                "stage apk to /data/local/tmp failed: exit=${stage.exitCode} " +
                    "out=${stage.stdout} err=${stage.stderr}",
            )
            return false
        }
        Log.i(TAG, "apk staged for root install size=${stage.stdout} path=$tmpApk")

        // stage 成功后再灭屏（prefs commit + bl_power），立刻启动 pm install
        ApkUpdateBrightnessCover.dimBeforeUpdate(context)

        val installBody =
            """
            #!/system/bin/sh
            echo install_start=${'$'}(date) >$tmpLog
            pm install -r -d $tmpApk >>$tmpLog 2>&1
            ec=${'$'}?
            echo exit=${'$'}ec >>$tmpLog
            if [ ${'$'}ec -eq 0 ]; then
              echo install_ok >>$tmpLog
            else
              echo install_failed >>$tmpLog
            fi
            """.trimIndent()

        val installB64 = Base64.encodeToString(installBody.toByteArray(StandardCharsets.UTF_8), Base64.NO_WRAP)
        val prepare = RootShell.run(
            "echo $installB64 | base64 -d > $installSh && " +
                "chmod 755 $installSh && " +
                "rm -f $tmpLog && " +
                "setsid /system/bin/sh $installSh </dev/null >/dev/null 2>&1 & " +
                "echo launched",
        )
        if (!prepare.isSuccess || !prepare.stdout.contains("launched")) {
            Log.w(TAG, "launch setsid install failed: ${prepare.stderr} ${prepare.stdout}")
            ApkUpdateBrightnessCover.restoreIfNeeded(context)
            return false
        }
        Log.i(TAG, "setsid root install launched")

        repeat(30) {
            Thread.sleep(400)
            val log = RootShell.run("cat $tmpLog 2>/dev/null").stdout
            if (log.contains("exit=") && !log.contains("exit=0")) {
                Log.w(TAG, "root pm install failed:\n$log")
                ApkUpdateBrightnessCover.restoreIfNeeded(context)
                return false
            }
            if (log.contains("Success") || log.contains("exit=0") || log.contains("install_ok")) {
                Log.i(TAG, "root pm install ok:\n$log")
                return true
            }
        }
        Log.i(TAG, "root install still running; relaunch alarms armed")
        return true
    }

    private fun installWithPackageInstaller(context: Context, apkFile: File) {
        val installer = context.packageManager.packageInstaller
        val params = PackageInstaller.SessionParams(
            PackageInstaller.SessionParams.MODE_FULL_INSTALL,
        ).apply {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                setInstallReason(PackageManager.INSTALL_REASON_USER)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                // API 34+：尽量不杀进程（普通自更新应用不一定生效，尽力而为）
                setDontKillApp(true)
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
            // 会话写入完成后再灭屏，紧接着 commit
            ApkUpdateBrightnessCover.dimBeforeUpdate(context)
            session.commit(pendingIntent.intentSender)
        } catch (error: Exception) {
            session.abandon()
            ApkUpdateBrightnessCover.restoreIfNeeded(context)
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
        // 真正拉起系统安装器前再灭屏
        ApkUpdateBrightnessCover.dimBeforeUpdate(context)
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
