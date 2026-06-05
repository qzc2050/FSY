package com.raydose.netshield.data

import android.content.Context
import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.Log
import kotlin.math.roundToInt

/** 本机显示与声音：仅 Android 系统层，与从机协议无关。 */
class DisplaySoundController(context: Context) {
    private val appContext = context.applicationContext
    private val audioManager =
        appContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val mainHandler = Handler(Looper.getMainLooper())

    data class SystemLevels(
        val brightness: Float,
        val systemVolume: Float,
    )

    fun readSystemLevels(): SystemLevels = SystemLevels(
        brightness = readBrightnessFraction(),
        systemVolume = readMusicVolumeFraction(),
    )

    fun applyBrightness(fraction: Float): Boolean {
        val value = (fraction.coerceIn(0f, 1f) * 255f).roundToInt().coerceIn(1, 255)
        return try {
            if (Settings.System.canWrite(appContext)) {
                Settings.System.putInt(
                    appContext.contentResolver,
                    Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL,
                )
                Settings.System.putInt(
                    appContext.contentResolver,
                    Settings.System.SCREEN_BRIGHTNESS,
                    value,
                )
                Log.i(TAG, "系统亮度已写入 value=$value")
                true
            } else {
                Log.w(TAG, "无 WRITE_SETTINGS 权限，无法写入系统亮度")
                false
            }
        } catch (e: Exception) {
            Log.e(TAG, "写入系统亮度失败", e)
            false
        }
    }

    fun applySystemVolume(fraction: Float): Boolean {
        return try {
            val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC).coerceAtLeast(1)
            val target = (fraction.coerceIn(0f, 1f) * max).roundToInt().coerceIn(0, max)
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, target, 0)
            Log.i(TAG, "系统音量已写入 stream=MUSIC level=$target/$max")
            true
        } catch (e: Exception) {
            Log.e(TAG, "写入系统音量失败", e)
            false
        }
    }

    /** 松手调节系统音量后播放短促提示音，便于听清当前音量（走 STREAM_MUSIC）。 */
    fun playSystemVolumePreview() {
        if (audioManager.getStreamVolume(AudioManager.STREAM_MUSIC) <= 0) return
        try {
            val tone = ToneGenerator(AudioManager.STREAM_MUSIC, 85)
            tone.startTone(ToneGenerator.TONE_PROP_BEEP2, 160)
            mainHandler.postDelayed({ tone.release() }, 200L)
        } catch (e: Exception) {
            Log.w(TAG, "音量预览音播放失败", e)
        }
    }

    private fun readBrightnessFraction(): Float {
        return try {
            val raw = Settings.System.getInt(
                appContext.contentResolver,
                Settings.System.SCREEN_BRIGHTNESS,
                128,
            )
            (raw / 255f).coerceIn(0f, 1f)
        } catch (e: Exception) {
            Log.w(TAG, "读取系统亮度失败", e)
            0.7f
        }
    }

    private fun readMusicVolumeFraction(): Float {
        return try {
            val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC).coerceAtLeast(1)
            val current = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
            (current.toFloat() / max).coerceIn(0f, 1f)
        } catch (e: Exception) {
            Log.w(TAG, "读取系统音量失败", e)
            0.7f
        }
    }

    companion object {
        private const val TAG = "NetShield"
    }
}
