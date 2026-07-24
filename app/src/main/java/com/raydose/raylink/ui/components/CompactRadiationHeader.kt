package com.raydose.raylink.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.model.SlaveProbeUi
import com.raydose.raylink.ui.theme.RaylinkAtmosphereTopBarBg
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

@Composable
fun CompactRadiationHeader(
    probes: List<SlaveProbeUi>,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .background(RaylinkAtmosphereTopBarBg)
            .padding(start = 32.dp, end = 56.dp),
        horizontalArrangement = Arrangement.spacedBy(48.dp, Alignment.Start),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (probes.isEmpty()) {
            CompactRadiationColumn(name = "—", doseRate = "---", unit = "μSv/h", online = false)
        } else {
            probes.forEach { probe ->
                CompactRadiationColumn(
                    name = probe.name,
                    doseRate = if (probe.isOnline) probe.doseRateText else "---",
                    unit = probe.doseUnit,
                    online = probe.isOnline,
                )
            }
        }
    }
}

@Composable
private fun CompactRadiationColumn(
    name: String,
    doseRate: String,
    unit: String,
    online: Boolean,
) {
    Column(
        modifier = Modifier.fillMaxHeight(),
        horizontalAlignment = Alignment.Start,
        verticalArrangement = Arrangement.Center,
    ) {
        Text(
            text = name,
            color = RaylinkTextSecondary,
            fontSize = 20.sp,
            lineHeight = 24.sp,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(modifier = Modifier.height(14.dp))
        Row(verticalAlignment = Alignment.Bottom) {
            Text(
                text = doseRate,
                color = if (online) RaylinkTextPrimary else RaylinkTextSecondary,
                fontSize = 34.sp,
                lineHeight = 38.sp,
            )
            Text(
                text = " $unit",
                color = RaylinkTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(bottom = 4.dp),
            )
        }
    }
}
