package com.raydose.raylink.data

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.provider.Settings
import android.util.Base64
import android.util.Log
import java.nio.charset.StandardCharsets

/**
 * APK 自更新时用 sysfs 关断背光，遮住进程被杀后短暂露出的桌面；
 * App 再次启动后恢复原先亮度与 bl_power。
 *
 * 优先 `/sys/class/backlight/backlight1/`（设备已验证）：
 * - `bl_power` 写 4 = POWERDOWN 关断背光；写 0 = UNBLANK 恢复
 * - `brightness` 写 0 可辅助灭屏，但 installPackageLI 杀进程切桌面时
 *   Display/PowerHAL 会把 brightness 盖掉，仅循环 echo 0 挡不住点亮。
 *
 * installPackageLI 杀进程后系统会自行亮屏：因此用 setsid 独立看门狗脚本
 * 持续钉 bl_power=4（兼 brightness=0），与 pm install 的 setsid 脚本并存。
 */
object ApkUpdateBrightnessCover {

    private const val TAG = "Raylink"
    private const val PREFS = "apk_update_brightness_cover"
    private const val KEY_PENDING = "pending"
    private const val KEY_BRIGHTNESS = "brightness"
    private const val KEY_MODE = "mode"
    private const val KEY_SYSFS_PATH = "sysfs_path"
    private const val KEY_SYSFS_VALUE = "sysfs_value"
    private const val KEY_BL_POWER_PATH = "bl_power_path"

    /** 已验证有效的工控背光亮度节点。 */
    private const val PRIMARY_BACKLIGHT_PATH =
        "/sys/class/backlight/backlight1/brightness"

    /** FB_BLANK_POWERDOWN：关断背光电源。 */
    private const val BL_POWER_POWERDOWN = 4
    /** FB_BLANK_UNBLANK：恢复背光。 */
    private const val BL_POWER_UNBLANK = 0

    /** 与 raylink_install.sh 分离，避免互相干扰。 */
    private const val HOLD_SCRIPT = "/data/local/tmp/raylink_bl_hold.sh"
    private const val STOP_FLAG = "/data/local/tmp/raylink_bl_restore"
    private const val HOLD_TIMEOUT_SEC = 60
    private const val HOLD_SLEEP_SEC = "0.05"
    /** 停看门狗后稍等，确保下一轮循环已看到 stop 标志。 */
    private const val STOP_SETTLE_MS = 120L

    fun dimBeforeUpdate(context: Context) {
        val appContext = context.applicationContext
        val prefs = appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        if (prefs.getBoolean(KEY_PENDING, false)) {
            // 上次未恢复：直接保持黑屏，避免覆盖已保存值
            val path = prefs.getString(KEY_SYSFS_PATH, null) ?: PRIMARY_BACKLIGHT_PATH
            val blPowerPath = prefs.getString(KEY_BL_POWER_PATH, null)
                ?: blPowerPathFromBrightness(path)
            applyBlPower(blPowerPath, BL_POWER_POWERDOWN)
            applySysfsBrightness(path, 0)
            applySystemBrightnessRaw(appContext, 0, forceManual = true)
            dimCurrentWindow(context)
            startBrightnessHoldWatchdog(path, blPowerPath)
            return
        }

        val current = readSystemBrightnessRaw(appContext)
        val mode = readSystemBrightnessMode(appContext)
        val sysfs = readSysfsBrightness()
        val holdPath = sysfs?.path ?: PRIMARY_BACKLIGHT_PATH
        val blPowerPath = blPowerPathFromBrightness(holdPath)
        val editor = prefs.edit()
            .putBoolean(KEY_PENDING, true)
            .putInt(KEY_BRIGHTNESS, current)
            .putInt(KEY_MODE, mode)
            .putString(KEY_BL_POWER_PATH, blPowerPath)
        if (sysfs != null) {
            editor.putString(KEY_SYSFS_PATH, sysfs.path)
                .putInt(KEY_SYSFS_VALUE, sysfs.value)
        } else {
            editor.remove(KEY_SYSFS_PATH).remove(KEY_SYSFS_VALUE)
        }
        // 进程马上会被杀（安装），必须同步落盘
        editor.commit()

        // 真正关断：先 bl_power=4，再 brightness=0；settings 仅顺带
        applyBlPower(blPowerPath, BL_POWER_POWERDOWN)
        applySysfsBrightness(sysfs?.path, 0)
        applySystemBrightnessRaw(appContext, 0, forceManual = true)
        dimCurrentWindow(context)
        // App 被 kill 后仍靠 setsid 看门狗钉 bl_power + brightness
        startBrightnessHoldWatchdog(holdPath, blPowerPath)
        Log.i(
            TAG,
            "APK update dim: saved brightness=$current mode=$mode " +
                "sysfs=${sysfs?.path}=${sysfs?.value} " +
                "bl_power=$blPowerPath=$BL_POWER_POWERDOWN (hold watchdog)",
        )
    }

    /**
     * 幂等恢复：pending 已清则无操作。
     * 安装失败路径可立刻调用；更新成功后由 MainActivity.onCreate 尽早调用以缩短黑屏。
     */
    fun restoreIfNeeded(context: Context) {
        val appContext = context.applicationContext
        val prefs = appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        if (!prefs.getBoolean(KEY_PENDING, false)) return

        val savedBrightness = prefs.getInt(KEY_BRIGHTNESS, 128).coerceIn(0, 255)
        val savedMode = prefs.getInt(KEY_MODE, Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL)
        val sysfsPath = prefs.getString(KEY_SYSFS_PATH, null)
        val sysfsValue = if (prefs.contains(KEY_SYSFS_VALUE)) {
            prefs.getInt(KEY_SYSFS_VALUE, -1)
        } else {
            -1
        }
        val blPowerPath = prefs.getString(KEY_BL_POWER_PATH, null)
            ?: sysfsPath?.let { blPowerPathFromBrightness(it) }
            ?: blPowerPathFromBrightness(PRIMARY_BACKLIGHT_PATH)

        // 先停看门狗并稍等；stop 标志保留到写回完成，避免竞态再被钉灭
        stopBrightnessHoldWatchdog(removeFlag = false)

        // UNBLANK 后再写回亮度
        applyBlPower(blPowerPath, BL_POWER_UNBLANK)
        if (sysfsPath != null && sysfsValue >= 0) {
            applySysfsBrightness(sysfsPath, sysfsValue)
        }
        applySystemBrightnessRaw(appContext, savedBrightness, forceManual = false, mode = savedMode)
        if (RootShell.isAvailable()) {
            RootShell.run("rm -f $STOP_FLAG 2>/dev/null")
        }
        prefs.edit().clear().commit()
        Log.i(
            TAG,
            "APK update brightness restored: brightness=$savedBrightness mode=$savedMode " +
                "sysfs=$sysfsPath=$sysfsValue bl_power=$blPowerPath=$BL_POWER_UNBLANK",
        )
    }

    /** 由 brightness 节点推导同目录 bl_power。 */
    private fun blPowerPathFromBrightness(brightnessPath: String): String {
        val trimmed = brightnessPath.trimEnd('/')
        return if (trimmed.endsWith("/brightness")) {
            trimmed.removeSuffix("/brightness") + "/bl_power"
        } else {
            // 兜底：假定 path 在 backlight 目录下
            val dir = trimmed.substringBeforeLast('/', trimmed)
            "$dir/bl_power"
        }
    }

    /**
     * setsid 启动独立背光钉灭脚本；进程与会话脱离 App，kill 后仍继续。
     * 优先 echo 4 > bl_power（POWERDOWN），兼 echo 0 > brightness。
     */
    private fun startBrightnessHoldWatchdog(brightnessPath: String, blPowerPath: String) {
        if (!RootShell.isAvailable()) return
        val path = brightnessPath.ifBlank { PRIMARY_BACKLIGHT_PATH }
        val powerPath = blPowerPath.ifBlank { blPowerPathFromBrightness(path) }
        val quotedBl = RootShell.quote(path)
        val quotedPower = RootShell.quote(powerPath)
        // 若有旧看门狗仍在跑，先发 stop 再起新的（pending 重入）
        RootShell.run("touch $STOP_FLAG 2>/dev/null")
        runCatching { Thread.sleep(80L) }

        // 60s / 0.05s ≈ 1200 次；也可用 stop 标志提前退出
        val maxLoops = HOLD_TIMEOUT_SEC * 20
        val scriptBody =
            """
            #!/system/bin/sh
            STOP=$STOP_FLAG
            BL=$quotedBl
            BP=$quotedPower
            i=0
            max=$maxLoops
            rm -f ${'$'}STOP
            while [ ${'$'}i -lt ${'$'}max ]; do
              if [ -f ${'$'}STOP ]; then
                rm -f ${'$'}STOP
                exit 0
              fi
              echo $BL_POWER_POWERDOWN > ${'$'}BP 2>/dev/null
              echo 0 > ${'$'}BL 2>/dev/null
              sleep $HOLD_SLEEP_SEC
              i=${'$'}((i+1))
            done
            """.trimIndent()

        val b64 = Base64.encodeToString(
            scriptBody.toByteArray(StandardCharsets.UTF_8),
            Base64.NO_WRAP,
        )
        val prepare = RootShell.run(
            "echo $b64 | base64 -d > $HOLD_SCRIPT && " +
                "chmod 755 $HOLD_SCRIPT && " +
                "rm -f $STOP_FLAG && " +
                "setsid /system/bin/sh $HOLD_SCRIPT </dev/null >/dev/null 2>&1 & " +
                "echo bl_hold_launched",
        )
        if (!prepare.isSuccess || !prepare.stdout.contains("bl_hold_launched")) {
            Log.w(
                TAG,
                "launch brightness hold watchdog failed: ${prepare.stderr} ${prepare.stdout}",
            )
            return
        }
        Log.i(
            TAG,
            "setsid brightness hold watchdog launched " +
                "bl_power=$powerPath=$BL_POWER_POWERDOWN brightness=$path=0 " +
                "timeout=${HOLD_TIMEOUT_SEC}s",
        )
    }

    /** 写 stop 标志并稍等，让看门狗退出后再由调用方写回亮度。 */
    private fun stopBrightnessHoldWatchdog(removeFlag: Boolean = true) {
        if (!RootShell.isAvailable()) return
        RootShell.run("touch $STOP_FLAG 2>/dev/null; chmod 666 $STOP_FLAG 2>/dev/null")
        runCatching { Thread.sleep(STOP_SETTLE_MS) }
        if (removeFlag) {
            RootShell.run("rm -f $STOP_FLAG 2>/dev/null")
        }
    }

    private fun dimCurrentWindow(context: Context) {
        val activity = context.findActivity() ?: return
        runCatching {
            activity.window.attributes = activity.window.attributes.apply {
                screenBrightness = 0f
            }
        }
    }

    private data class SysfsBrightness(val path: String, val value: Int)

    /**
     * 有 root 时读背光节点当前值（保存用）。
     * 优先 backlight1；否则扫描 /sys/class/backlight 下各节点 brightness，
     * 优先 actual 非 0、或名字含 backlight1。
     */
    private fun readSysfsBrightness(): SysfsBrightness? {
        if (!RootShell.isAvailable()) return null

        // 1) 已验证路径
        readSysfsAt(PRIMARY_BACKLIGHT_PATH)?.let { return it }

        // 2) 扫描其它 backlight 节点并按优先级选
        val discovered = RootShell.run(
            "for f in /sys/class/backlight/*/brightness; do " +
                "[ -f \"\$f\" ] && echo \"\$f\"; done",
        ).stdout.lineSequence().map { it.trim() }.filter { it.isNotEmpty() }.toList()

        val ranked = discovered
            .filter { it != PRIMARY_BACKLIGHT_PATH }
            .mapNotNull { path ->
                val value = catSysfsInt(path) ?: return@mapNotNull null
                val actual = catSysfsInt(path.replace("/brightness", "/actual_brightness"))
                val score = when {
                    path.contains("backlight1", ignoreCase = true) -> 3
                    actual != null && actual > 0 -> 2
                    value > 0 -> 1
                    else -> 0
                }
                Triple(path, value, score)
            }
            .sortedByDescending { it.third }

        val best = ranked.firstOrNull()
        if (best != null) {
            Log.i(TAG, "APK update sysfs brightness read: ${best.first}=${best.second}")
            return SysfsBrightness(best.first, best.second)
        }
        Log.w(TAG, "APK update: no readable backlight sysfs node")
        return null
    }

    private fun readSysfsAt(path: String): SysfsBrightness? {
        val value = catSysfsInt(path) ?: return null
        Log.i(TAG, "APK update sysfs brightness read: $path=$value")
        return SysfsBrightness(path, value)
    }

    private fun catSysfsInt(path: String): Int? {
        val quoted = RootShell.quote(path)
        val out = RootShell.run("cat $quoted 2>/dev/null").stdout.trim()
        return out.toIntOrNull()
    }

    /** 写 bl_power（4=POWERDOWN / 0=UNBLANK）。 */
    private fun applyBlPower(path: String, value: Int) {
        if (!RootShell.isAvailable()) return
        if (writeSysfsInt(path, value)) {
            Log.i(TAG, "root set bl_power $path=$value")
            return
        }
        Log.w(TAG, "root bl_power failed path=$path value=$value")
    }

    /** 写 sysfs 亮度；path 为空则用 PRIMARY 再扫描。 */
    private fun applySysfsBrightness(preferredPath: String?, value: Int) {
        if (!RootShell.isAvailable()) return

        val paths = buildList {
            if (!preferredPath.isNullOrBlank()) add(preferredPath)
            add(PRIMARY_BACKLIGHT_PATH)
            addAll(
                RootShell.run(
                    "for f in /sys/class/backlight/*/brightness; do " +
                        "[ -f \"\$f\" ] && echo \"\$f\"; done",
                ).stdout.lineSequence().map { it.trim() }.filter { it.isNotEmpty() },
            )
        }.distinct()

        for (path in paths) {
            if (writeSysfsBrightness(path, value)) return
        }
        Log.w(TAG, "APK update: failed to write backlight sysfs to $value")
    }

    private fun writeSysfsBrightness(path: String, value: Int): Boolean {
        if (writeSysfsInt(path, value)) {
            Log.i(TAG, "root set sysfs brightness $path=$value")
            return true
        }
        Log.w(TAG, "root sysfs brightness failed path=$path")
        return false
    }

    private fun writeSysfsInt(path: String, value: Int): Boolean {
        if (!RootShell.isAvailable()) return false
        val quoted = RootShell.quote(path)
        // 部分节点需先 chmod；失败也继续 echo（设备验证：echo 直接写可行）
        val result = RootShell.run(
            "chmod 666 $quoted 2>/dev/null; echo $value > $quoted",
        )
        if (!result.isSuccess) {
            Log.w(TAG, "root sysfs write failed path=$path: ${result.stderr}")
        }
        return result.isSuccess
    }

    private fun readSystemBrightnessRaw(context: Context): Int {
        if (RootShell.isAvailable()) {
            val out = RootShell.run("settings get system screen_brightness").stdout.trim()
            out.toIntOrNull()?.let { return it.coerceIn(0, 255) }
        }
        return try {
            Settings.System.getInt(
                context.contentResolver,
                Settings.System.SCREEN_BRIGHTNESS,
                128,
            ).coerceIn(0, 255)
        } catch (_: Exception) {
            128
        }
    }

    private fun readSystemBrightnessMode(context: Context): Int {
        if (RootShell.isAvailable()) {
            val out = RootShell.run("settings get system screen_brightness_mode").stdout.trim()
            out.toIntOrNull()?.let { return it }
        }
        return try {
            Settings.System.getInt(
                context.contentResolver,
                Settings.System.SCREEN_BRIGHTNESS_MODE,
                Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL,
            )
        } catch (_: Exception) {
            Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL
        }
    }

    private fun applySystemBrightnessRaw(
        context: Context,
        brightness: Int,
        forceManual: Boolean,
        mode: Int = Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL,
    ) {
        val value = brightness.coerceIn(0, 255)
        if (RootShell.isAvailable()) {
            val cmd = if (forceManual) {
                "settings put system screen_brightness_mode 0 && " +
                    "settings put system screen_brightness $value"
            } else {
                "settings put system screen_brightness $value && " +
                    "settings put system screen_brightness_mode $mode"
            }
            val result = RootShell.run(cmd)
            if (result.isSuccess) {
                Log.i(TAG, "root set brightness=$value forceManual=$forceManual mode=$mode")
                return
            }
            Log.w(TAG, "root brightness failed: ${result.stderr}")
        }

        try {
            if (!Settings.System.canWrite(context)) {
                Log.w(TAG, "no WRITE_SETTINGS; settings brightness skipped")
                return
            }
            if (forceManual) {
                Settings.System.putInt(
                    context.contentResolver,
                    Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL,
                )
            } else {
                Settings.System.putInt(
                    context.contentResolver,
                    Settings.System.SCREEN_BRIGHTNESS_MODE,
                    mode,
                )
            }
            Settings.System.putInt(
                context.contentResolver,
                Settings.System.SCREEN_BRIGHTNESS,
                value,
            )
        } catch (e: Exception) {
            Log.w(TAG, "apply system brightness failed", e)
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
