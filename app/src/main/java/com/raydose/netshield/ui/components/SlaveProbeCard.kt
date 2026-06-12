package com.raydose.netshield.ui.components

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.R
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.theme.NetShieldAlarmActive
import com.raydose.netshield.ui.theme.NetShieldCardOffline
import com.raydose.netshield.ui.theme.NetShieldCardOnlineEnd
import com.raydose.netshield.ui.theme.NetShieldCardOnlineStart
import com.raydose.netshield.ui.theme.NetShieldDoorOpen
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

private const val ALARM_FLASH_MS = 600

/** 第三行：探头卡片 */
@Composable
fun SlaveProbeCard(
    probe: SlaveProbeUi,
    onDetailClick: () -> Unit,
    modifier: Modifier = Modifier,
    standbyFrosted: Boolean = false,
    slotSize: ProbeCardSlotSize = ProbeCardSlotSize.Full,
) {
    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val spec = probeCardSlotSpec(
            slotSize = slotSize,
            standbyFrosted = standbyFrosted,
            cardWidthDp = maxWidth.value,
            cardHeightDp = maxHeight.value,
        )
        SlaveProbeCardContent(
            probe = probe,
            onDetailClick = onDetailClick,
            spec = spec,
            standbyFrosted = standbyFrosted,
        )
    }
}

@Composable
private fun SlaveProbeCardContent(
    probe: SlaveProbeUi,
    onDetailClick: () -> Unit,
    spec: ProbeCardSlotSpec,
    standbyFrosted: Boolean,
) {
    val alarmFlashAlpha = rememberProbeAlarmFlashAlpha(enabled = probe.hasAlarm)
    val corner = spec.cornerDp.dp
    val cardBrush = when {
        probe.hasAlarm && !standbyFrosted -> Brush.horizontalGradient(
            listOf(
                NetShieldAlarmActive.copy(alpha = alarmFlashAlpha),
                NetShieldDoorOpen.copy(alpha = alarmFlashAlpha * 0.88f),
            ),
        )
        standbyFrosted -> Brush.linearGradient(listOf(Color.Transparent, Color.Transparent))
        probe.isOnline -> Brush.horizontalGradient(listOf(NetShieldCardOnlineStart, NetShieldCardOnlineEnd))
        else -> Brush.horizontalGradient(listOf(NetShieldCardOffline, NetShieldCardOffline.copy(alpha = 0.85f)))
    }
    val envBarAlpha = when {
        !spec.showEnvBar -> 0f
        standbyFrosted -> 0.35f
        else -> 0.25f
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .then(
                if (standbyFrosted) {
                    Modifier
                } else {
                    Modifier
                        .clip(RoundedCornerShape(corner))
                        .background(cardBrush)
                },
            ),
    ) {
        if (standbyFrosted && probe.hasAlarm) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .clip(RoundedCornerShape(corner))
                    .background(NetShieldAlarmActive.copy(alpha = alarmFlashAlpha * 0.55f)),
            )
        }

        Column(modifier = Modifier.fillMaxSize()) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(if (spec.showEnvBar) 1f - spec.envWeight else 1f)
                    .padding(
                        horizontal = spec.contentPaddingH.dp,
                        vertical = spec.contentPaddingV.dp,
                    ),
            ) {
                if (probe.hasAlarm) {
                    Icon(
                        painter = painterResource(R.drawable.ic_log_alarm),
                        contentDescription = "报警",
                        tint = NetShieldAlarmActive,
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .size(spec.alarmSp.dp),
                    )
                }

                Row(
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .padding(top = 2.dp)
                        .fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.Center,
                ) {
                    Text(
                        text = probe.name,
                        color = NetShieldTextPrimary,
                        fontSize = spec.nameSp.sp,
                        fontWeight = FontWeight.Medium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                    if (!standbyFrosted) {
                        Text(
                            text = "🔍",
                            fontSize = spec.detailSp.sp,
                            modifier = Modifier
                                .padding(start = 6.dp)
                                .clickable(onClick = onDetailClick),
                        )
                    }
                }

                Row(
                    modifier = Modifier
                        .align(Alignment.Center)
                        .offset(y = spec.doseOffsetDp.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.Center,
                ) {
                    Text(
                        text = probe.doseRateText,
                        color = NetShieldTextPrimary,
                        fontSize = spec.doseSp.sp,
                        fontWeight = FontWeight.Light,
                        lineHeight = spec.doseSp.sp,
                        maxLines = 1,
                    )
                    Text(
                        text = probe.doseUnit,
                        color = NetShieldTextSecondary,
                        fontSize = spec.doseUnitSp.sp,
                        modifier = Modifier.padding(start = spec.doseUnitGapDp.dp),
                    )
                }
            }

            if (spec.showEnvBar) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(spec.envWeight)
                        .clip(
                            RoundedCornerShape(
                                topStart = 0.dp,
                                topEnd = 0.dp,
                                bottomEnd = corner,
                                bottomStart = corner,
                            ),
                        )
                        .background(Color.Black.copy(alpha = envBarAlpha))
                        .padding(
                            horizontal = spec.envPaddingH.dp,
                            vertical = spec.envPaddingV.dp,
                        ),
                    contentAlignment = Alignment.Center,
                ) {
                    if (probe.isOnline) {
                        ProbeEnvBar(
                            probe = probe,
                            spec = spec,
                        )
                    } else {
                        Text(text = "...", color = NetShieldTextSecondary, fontSize = spec.offlineSp.sp)
                    }
                }
            }
        }
    }
}

@Composable
private fun rememberProbeAlarmFlashAlpha(enabled: Boolean): Float {
    if (!enabled) return 0f
    val transition = rememberInfiniteTransition(label = "probeAlarmFlash")
    val alpha by transition.animateFloat(
        initialValue = 0.45f,
        targetValue = 0.95f,
        animationSpec = infiniteRepeatable(
            animation = tween(ALARM_FLASH_MS),
            repeatMode = RepeatMode.Reverse,
        ),
        label = "probeAlarmFlashAlpha",
    )
    return alpha
}

@Composable
private fun ProbeEnvBar(
    probe: SlaveProbeUi,
    spec: ProbeCardSlotSpec,
) {
    when (spec.envLayout) {
        EnvLayout.SingleRow -> {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                spec.envItems.forEach { (label, value) ->
                    EnvChip(label, value(probe), spec.envSp)
                }
            }
        }
        EnvLayout.MultiRow -> {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(spec.envRowGapDp.dp, Alignment.CenterVertically),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                spec.envItems.chunked(spec.envItemsPerRow.coerceAtLeast(1)).forEach { rowItems ->
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceEvenly,
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        rowItems.forEach { (label, value) ->
                            EnvChip(label, value(probe), spec.envSp)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun EnvChip(label: String, value: String, fontSp: Int) {
    Row(verticalAlignment = Alignment.Bottom) {
        Text(
            text = "$label:",
            color = NetShieldTextSecondary,
            fontSize = fontSp.sp,
            maxLines = 1,
        )
        Text(
            text = " $value",
            color = NetShieldTextPrimary,
            fontSize = fontSp.sp,
            fontWeight = FontWeight.Medium,
            maxLines = 1,
        )
    }
}
