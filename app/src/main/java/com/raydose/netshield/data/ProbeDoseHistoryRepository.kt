package com.raydose.netshield.data

import android.content.Context
import com.raydose.netshield.model.DailyDoseSummary
import com.raydose.netshield.model.ProbeDoseSample
import org.json.JSONArray
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale
import java.util.concurrent.TimeUnit

/**
 * 探头 5 分钟剂量历史本地存储。
 * 当前无真实历史时自动生成模拟采样并落盘，后续由 [recordSampleIfDue] 持续追加。
 */
class ProbeDoseHistoryRepository(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private val dateFmt = SimpleDateFormat("yyyy-MM-dd", Locale.getDefault())

    fun recordSampleIfDue(probeId: String, doseRateUsvH: Double, nowMillis: Long = System.currentTimeMillis()) {
        if (doseRateUsvH <= 0.0) return
        val samples = loadSamples(probeId)
        val lastAt = samples.maxOfOrNull { it.recordedAtMillis } ?: 0L
        if (nowMillis - lastAt < SAMPLE_INTERVAL_MS) return
        val updated = samples + ProbeDoseSample(probeId, doseRateUsvH, nowMillis)
        saveSamples(probeId, updated)
    }

    fun dailySummaries(probeId: String, fallbackBaseRateUsvH: Double, dayCount: Int = 7): List<DailyDoseSummary> {
        var samples = loadSamples(probeId)
        if (samples.isEmpty()) {
            samples = buildSimulatedSamples(probeId, fallbackBaseRateUsvH, dayCount)
            saveSamples(probeId, samples)
        }
        return aggregateDaily(samples, dayCount)
    }

    private fun loadSamples(probeId: String): List<ProbeDoseSample> {
        val root = prefs.getString(KEY_SAMPLES, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: return emptyList()
        val arr = root.optJSONArray(probeId) ?: return emptyList()
        return buildList(arr.length()) {
            for (i in 0 until arr.length()) {
                val o = arr.optJSONObject(i) ?: continue
                add(
                    ProbeDoseSample(
                        probeId = probeId,
                        doseRateUsvH = o.optDouble("d", 0.0),
                        recordedAtMillis = o.optLong("t", 0L),
                    ),
                )
            }
        }
    }

    private fun saveSamples(probeId: String, samples: List<ProbeDoseSample>) {
        val root = prefs.getString(KEY_SAMPLES, null)?.let { runCatching { JSONObject(it) }.getOrNull() }
            ?: JSONObject()
        val arr = JSONArray()
        samples.forEach { sample ->
            arr.put(
                JSONObject().apply {
                    put("t", sample.recordedAtMillis)
                    put("d", sample.doseRateUsvH)
                },
            )
        }
        root.put(probeId, arr)
        prefs.edit().putString(KEY_SAMPLES, root.toString()).apply()
    }

    private fun aggregateDaily(samples: List<ProbeDoseSample>, dayCount: Int): List<DailyDoseSummary> {
        val today = Calendar.getInstance()
        val grouped = samples.groupBy { dateFmt.format(it.recordedAtMillis) }
        return buildList {
            for (i in 0 until dayCount) {
                val cal = today.clone() as Calendar
                cal.add(Calendar.DAY_OF_MONTH, -i)
                val dateText = dateFmt.format(cal.time)
                val daySamples = grouped[dateText].orEmpty()
                val accum = daySamples.sumOf { it.doseRateUsvH * (SAMPLE_INTERVAL_MINUTES / 60.0) }
                add(
                    DailyDoseSummary(
                        dateText = dateText,
                        accumDoseText = "${"%.2f".format(accum)} uSv",
                    ),
                )
            }
        }
    }

    private fun buildSimulatedSamples(
        probeId: String,
        baseRateUsvH: Double,
        dayCount: Int,
    ): List<ProbeDoseSample> {
        val rate = if (baseRateUsvH <= 0.0) 0.08 else baseRateUsvH
        val end = System.currentTimeMillis()
        val start = end - TimeUnit.DAYS.toMillis(dayCount.toLong())
        val samples = mutableListOf<ProbeDoseSample>()
        var cursor = alignToFiveMinuteBucket(start)
        var index = 0
        while (cursor <= end) {
            val drift = 0.95 - (index % 48) * 0.003
            val value = (rate * drift).coerceAtLeast(0.02)
            samples += ProbeDoseSample(probeId, value, cursor)
            cursor += SAMPLE_INTERVAL_MS
            index++
        }
        return samples
    }

    private fun alignToFiveMinuteBucket(millis: Long): Long {
        val bucket = TimeUnit.MINUTES.toMillis(SAMPLE_INTERVAL_MINUTES.toLong())
        return millis - (millis % bucket)
    }

    companion object {
        private const val PREFS_NAME = "probe_dose_history"
        private const val KEY_SAMPLES = "samples_v1"
        private const val SAMPLE_INTERVAL_MINUTES = 5
        private val SAMPLE_INTERVAL_MS = TimeUnit.MINUTES.toMillis(SAMPLE_INTERVAL_MINUTES.toLong())
    }
}
