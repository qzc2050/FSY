package com.raydose.raylink.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

@Composable
fun SystemClockHeader(
    systemName: String,
    dateText: String,
    timeText: String,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier) {
        Text(
            text = systemName,
            color = RaylinkTextPrimary,
            fontSize = 26.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Text(
            text = dateText,
            color = RaylinkTextSecondary,
            fontSize = 18.sp,
            modifier = Modifier.padding(top = 4.dp),
        )
        Text(
            text = timeText,
            color = RaylinkTextPrimary,
            fontSize = 48.sp,
            fontWeight = FontWeight.Light,
            modifier = Modifier.padding(top = 2.dp),
        )
    }
}

@Composable
fun HostEnvTicker(
    readings: List<Pair<String, String>>,
    modifier: Modifier = Modifier,
) {
    if (readings.isEmpty()) return
    Column(
        modifier = modifier,
        horizontalAlignment = Alignment.End,
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        readings.forEach { (label, value) ->
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(text = "$label:", color = RaylinkTextSecondary, fontSize = 18.sp)
                Text(text = value, color = RaylinkTextPrimary, fontSize = 18.sp)
            }
        }
    }
}

@Composable
fun StatusIconsRow(
    modifier: Modifier = Modifier,
    bluetoothOnline: Boolean = false,
    ethernetOnline: Boolean = false,
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.End,
    ) {
        ConnectivityStatusIconsRow(
            bluetoothOnline = bluetoothOnline,
            ethernetOnline = ethernetOnline,
        )
    }
}
