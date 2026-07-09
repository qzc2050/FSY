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
 * 探头 5 分钟累计剂量历史本地存储。
 * 样本单位为 μSv（一窗 D5），日累计 = Σ 当天 doseUsv。
 */
class ProbeDoseHistoryRepository(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private val dateFmt = SimpleDateFormat("yyyy-MM-dd", Locale.getDefault())

    fun recordSampleIfDue(probeId: String, doseUsv: Double, recordedAtMillis: Long = System.currentTimeMillis()) {
        if (doseUsv <= 0.0) return
        val samples = loadSamples(probeId)
        val bucket = alignToFiveMinuteBucket(recordedAtMillis)
        /* 同一 5min 桶已有样本则跳过；避免手机时间/设备时间混用导致误拒 */
        if (samples.any { alignToFiveMinuteBucket(it.recordedAtMillis) == bucket }) return
        val updated = samples + ProbeDoseSample(probeId, doseUsv, recordedAtMillis)
        saveSamples(probeId, updated)
    }

    fun dailySummaries(probeId: String, fallbackBaseRateUsvH: Double, dayCount: Int = 7): List<DailyDoseSummary> {
        val samples = loadSamples(probeId)
        /* 无真实样本时仅内存模拟展示，不落盘，避免占满 5min 桶导致真帧被拒 */
        val forAgg = if (samples.isEmpty()) {
            buildSimulatedSamples(probeId, fallbackBaseRateUsvH, dayCount)
        } else {
            samples
        }
        return aggregateDaily(forAgg, dayCount)
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
                        doseUsv = o.optDouble("d", 0.0),
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
                    put("d", sample.doseUsv)
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
                val accum = daySamples.sumOf { it.doseUsv }
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
        /* 无真实 5min 帧时：用剂量率估算每窗累计 μSv ≈ rate × 5/60 */
        val rate = if (baseRateUsvH <= 0.0) 0.08 else baseRateUsvH
        val end = System.currentTimeMillis()
        val start = end - TimeUnit.DAYS.toMillis(dayCount.toLong())
        val samples = mutableListOf<ProbeDoseSample>()
        var cursor = alignToFiveMinuteBucket(start)
        var index = 0
        while (cursor <= end) {
            val drift = 0.95 - (index % 48) * 0.003
            val doseUsv = (rate * (SAMPLE_INTERVAL_MINUTES / 60.0) * drift).coerceAtLeast(0.001)
            samples += ProbeDoseSample(probeId, doseUsv, cursor)
            cursor += SAMPLE_INTERVAL_MS
            index++
        }
        return samples
    }

    private fun alignToFiveMinuteBucket(millis: Long): Long {
        return millis - (millis % SAMPLE_INTERVAL_MS)
    }

    companion object {
        private const val PREFS_NAME = "probe_dose_history"
        /** v2：不再把模拟样本写入 prefs；旧 v1 若已污染可清应用数据或换 key */
        private const val KEY_SAMPLES = "samples_v2"
        /**
         * 与固件 DOSE_5MIN_TEST_FAST 对齐：
         * true → 5 秒一窗（联调）；false → 正式 5 分钟。量产务必改 false。
         */
        private const val DOSE_5MIN_TEST_FAST = false
        private const val SAMPLE_INTERVAL_MINUTES = 5
        private val SAMPLE_INTERVAL_MS =
            if (DOSE_5MIN_TEST_FAST) {
                TimeUnit.SECONDS.toMillis(5)
            } else {
                TimeUnit.MINUTES.toMillis(SAMPLE_INTERVAL_MINUTES.toLong())
            }
    }
}
