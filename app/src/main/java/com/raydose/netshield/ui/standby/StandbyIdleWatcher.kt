package com.raydose.netshield.ui.standby

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay

private const val IDLE_CHECK_INTERVAL_MS = 1_000L

/**
 * 主页无操作超过 [standbyMinutes] 后触发 [onEnterStandby]。
 * [standbyMinutes] ≤ 0（设置里「永不」）时不启用。
 * [watchIdle] 为 false 时暂停计时（子页面、已处于待机等）。
 */
@Composable
fun StandbyIdleWatcher(
    watchIdle: Boolean,
    standbyMinutes: Int,
    onRegisterUserInteraction: (listener: (() -> Unit)?) -> Unit,
    onEnterStandby: () -> Unit,
) {
    var lastInteractionAt by remember { mutableLongStateOf(System.currentTimeMillis()) }
    val watchState = rememberUpdatedState(watchIdle)
    val minutesState = rememberUpdatedState(standbyMinutes)
    val onEnterState = rememberUpdatedState(onEnterStandby)

    LaunchedEffect(watchIdle) {
        if (watchIdle) {
            lastInteractionAt = System.currentTimeMillis()
        }
    }

    DisposableEffect(onRegisterUserInteraction) {
        onRegisterUserInteraction {
            if (watchState.value && minutesState.value > 0) {
                lastInteractionAt = System.currentTimeMillis()
            }
        }
        onDispose {
            onRegisterUserInteraction(null)
        }
    }

    LaunchedEffect(watchIdle, standbyMinutes) {
        if (!watchIdle || standbyMinutes <= 0) return@LaunchedEffect
        val timeoutMs = standbyMinutes * 60_000L
        while (true) {
            delay(IDLE_CHECK_INTERVAL_MS)
            if (System.currentTimeMillis() - lastInteractionAt >= timeoutMs) {
                onEnterState.value()
                return@LaunchedEffect
            }
        }
    }
}
