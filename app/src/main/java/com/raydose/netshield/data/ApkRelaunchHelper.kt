package com.raydose.netshield.data

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.SystemClock
import com.raydose.netshield.MainActivity

/**
 * 覆盖安装后尽快回到本应用。
 *
 * 更新时系统会先杀掉进程并短暂露出桌面；从 BroadcastReceiver 直接 startActivity
 * 在部分 ROM 上会被延迟。这里同时用 AlarmManager + PendingIntent 多次尝试拉起。
 */
object ApkRelaunchHelper {

    private const val ACTION_RELAUNCH = "com.raydose.netshield.action.RELAUNCH_AFTER_UPDATE"
    private const val BASE_REQUEST_CODE = 9100
    private const val MAX_ALARM_SLOTS = 8

    /** 安装完成回调里调用：立即拉起并安排短间隔重试。 */
    fun relaunchAfterUpdate(context: Context) {
        val appContext = context.applicationContext
        cancelScheduledRelaunch(appContext)
        relaunchNow(appContext)
        scheduleRelaunchAlarms(
            appContext,
            delaysMs = longArrayOf(150, 400, 900, 1800),
        )
    }

    fun cancelScheduledRelaunch(context: Context) {
        val alarmManager = context.getSystemService(AlarmManager::class.java) ?: return
        val appContext = context.applicationContext
        for (index in 0 until MAX_ALARM_SLOTS) {
            alarmManager.cancel(buildActivityPendingIntent(appContext, BASE_REQUEST_CODE + index))
        }
    }

    private fun relaunchNow(context: Context) {
        runCatching { context.startActivity(launchIntent(context)) }
    }

    private fun scheduleRelaunchAlarms(context: Context, delaysMs: LongArray) {
        val alarmManager = context.getSystemService(AlarmManager::class.java) ?: return
        val triggerBase = SystemClock.elapsedRealtime()
        delaysMs.take(MAX_ALARM_SLOTS).forEachIndexed { index, delayMs ->
            alarmManager.set(
                AlarmManager.ELAPSED_REALTIME_WAKEUP,
                triggerBase + delayMs,
                buildActivityPendingIntent(context, BASE_REQUEST_CODE + index),
            )
        }
    }

    private fun launchIntent(context: Context): Intent =
        Intent(context, MainActivity::class.java).apply {
            action = ACTION_RELAUNCH
            addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK or
                    Intent.FLAG_ACTIVITY_CLEAR_TOP or
                    Intent.FLAG_ACTIVITY_SINGLE_TOP or
                    Intent.FLAG_ACTIVITY_NO_ANIMATION,
            )
        }

    private fun buildActivityPendingIntent(context: Context, requestCode: Int): PendingIntent {
        val flags = PendingIntent.FLAG_UPDATE_CURRENT or pendingIntentMutabilityFlag()
        return PendingIntent.getActivity(
            context,
            requestCode,
            launchIntent(context),
            flags,
        )
    }

    private fun pendingIntentMutabilityFlag(): Int =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            PendingIntent.FLAG_IMMUTABLE
        } else {
            0
        }
}
