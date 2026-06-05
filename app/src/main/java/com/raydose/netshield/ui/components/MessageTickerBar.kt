package com.raydose.netshield.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldMessageBar
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun MessageTickerBar(
    previewText: String,
    messageCount: Int,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    widthFraction: Float = 0.54f,
) {
    Row(
        modifier = modifier
            .fillMaxWidth(widthFraction)
            .clip(RoundedCornerShape(24.dp))
            .background(NetShieldMessageBar)
            .clickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(text = "🎤", fontSize = 22.sp)
        Text(
            text = previewText,
            color = NetShieldTextPrimary,
            fontSize = 18.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier
                .weight(1f)
                .padding(horizontal = 12.dp),
        )
        Text(text = "$messageCount", color = NetShieldTextSecondary, fontSize = 18.sp)
        Text(text = "  ▲", color = NetShieldTextSecondary, fontSize = 16.sp)
    }
}
