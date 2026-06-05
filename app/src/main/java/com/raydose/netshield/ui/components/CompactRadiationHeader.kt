package com.raydose.netshield.ui.components

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
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.theme.NetShieldAtmosphereTopBarBg
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun CompactRadiationHeader(
    probes: List<SlaveProbeUi>,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .background(NetShieldAtmosphereTopBarBg)
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
            color = NetShieldTextSecondary,
            fontSize = 20.sp,
            lineHeight = 24.sp,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(modifier = Modifier.height(14.dp))
        Row(verticalAlignment = Alignment.Bottom) {
            Text(
                text = doseRate,
                color = if (online) NetShieldTextPrimary else NetShieldTextSecondary,
                fontSize = 34.sp,
                lineHeight = 38.sp,
            )
            Text(
                text = " $unit",
                color = NetShieldTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(bottom = 4.dp),
            )
        }
    }
}
