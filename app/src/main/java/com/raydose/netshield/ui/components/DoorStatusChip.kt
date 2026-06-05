package com.raydose.netshield.ui.components

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.DoorState
import com.raydose.netshield.ui.theme.NetShieldDoorClosed
import com.raydose.netshield.ui.theme.NetShieldDoorOpen

@Composable
fun DoorStatusChip(
    doorState: DoorState,
    modifier: Modifier = Modifier,
) {
    val (text, color) = when (doorState) {
        DoorState.Open -> "已开门" to NetShieldDoorOpen
        DoorState.Closed -> "已关门" to NetShieldDoorClosed
        DoorState.Unknown -> "门状态未知" to Color.Gray
    }
    Row(modifier = modifier, verticalAlignment = Alignment.CenterVertically) {
        Text(text = "🚪", fontSize = 26.sp)
        Text(text = text, color = color, fontSize = 20.sp, modifier = Modifier.padding(start = 8.dp))
    }
}
