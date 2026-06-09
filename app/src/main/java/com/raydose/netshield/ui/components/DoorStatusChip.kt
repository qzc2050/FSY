package com.raydose.netshield.ui.components

import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.R
import com.raydose.netshield.model.DoorState
import com.raydose.netshield.ui.theme.NetShieldDoorClosed
import com.raydose.netshield.ui.theme.NetShieldDoorOpen
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun DoorStatusChip(
    doorState: DoorState,
    modifier: Modifier = Modifier,
) {
    val (text, color, iconRes) = when (doorState) {
        DoorState.Open -> Triple("已开门", NetShieldDoorOpen, R.drawable.ic_door_open)
        DoorState.Closed -> Triple("已关门", NetShieldDoorClosed, R.drawable.ic_door_closed)
        DoorState.Unknown -> Triple("门状态未知", NetShieldTextSecondary, R.drawable.ic_door_closed)
    }
    Row(modifier = modifier, verticalAlignment = Alignment.CenterVertically) {
        // 关门白线、开门红线，透明底 PNG 不可 tint
        DoorStatusIcon(res = iconRes)
        Text(text = text, color = color, fontSize = 20.sp, modifier = Modifier.padding(start = 8.dp))
    }
}

@Composable
private fun DoorStatusIcon(
    @DrawableRes res: Int,
    modifier: Modifier = Modifier,
) {
    Image(
        painter = painterResource(res),
        contentDescription = null,
        contentScale = ContentScale.Fit,
        modifier = modifier.size(36.dp),
    )
}
