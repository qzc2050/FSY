package com.raydose.netshield.data

import android.content.Context
import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Handler
import android.os.Looper
import android.util.Log
import kotlin.math.roundToInt

/**
 * 本机报警音：任一探头辐射上/下阈值报警（alarm_bit bit0/bit1）时循环播放，
 * 受「静音」与「暂停报警 5 分钟」控制。
 */
class HostAlarmController(context: Context) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private var toneGenerator: ToneGenerator? = null
    private var playing = false
    private var activeVolumeFraction = 0.7f

    private val pulseRunnable = object : Runnable {
        override fun run() {
            if (!playing) return
            try {
                toneGenerator?.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, PULSE_MS)
            } catch (e: Exception) {
                Log.w(TAG, "报警音脉冲失败", e)
            }
            mainHandler.postDelayed(this, PULSE_INTERVAL_MS)
        }
    }

    fun sync(
        shouldPlay: Boolean,
        volumeFraction: Float,
        alarmSuppressed: Boolean,
    ) {
        val wantPlay = shouldPlay && !alarmSuppressed
        val vol = volumeFraction.coerceIn(0f, 1f)
        if (wantPlay) {
            if (!playing || vol != activeVolumeFraction) {
                stopInternal()
                startInternal(vol)
            }
        } else if (playing) {
            stopInternal()
        }
    }

    fun release() {
        stopInternal()
    }

    private fun startInternal(volumeFraction: Float) {
        if (volumeFraction <= 0f) return
        activeVolumeFraction = volumeFraction
        val vol = (volumeFraction * 100f).roundToInt().coerceIn(1, 100)
        try {
            toneGenerator = ToneGenerator(AudioManager.STREAM_ALARM, vol)
            playing = true
            mainHandler.post(pulseRunnable)
            Log.i(TAG, "本机报警音已开始 vol=$vol")
        } catch (e: Exception) {
            Log.e(TAG, "本机报警音启动失败", e)
            playing = false
            toneGenerator?.release()
            toneGenerator = null
        }
    }

    private fun stopInternal() {
        if (!playing) return
        playing = false
        mainHandler.removeCallbacks(pulseRunnable)
        toneGenerator?.release()
        toneGenerator = null
        Log.i(TAG, "本机报警音已停止")
    }

    companion object {
        private const val TAG = "NetShield"
        private const val PULSE_MS = 480
        private const val PULSE_INTERVAL_MS = 1_400L
    }
}
