package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldSettingsContentBg
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import kotlinx.coroutines.delay

/** 报警音量与分页「1/1」之间的保存成功提示，1 秒后自动消失。 */
@Composable
fun SaveSuccessToast(onDismiss: () -> Unit) {
    LaunchedEffect(Unit) {
        delay(1000)
        onDismiss()
    }
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(NetShieldSettingsContentBg)
            .padding(horizontal = 18.dp, vertical = 8.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = "保存成功",
            color = NetShieldTextPrimary,
            fontSize = 16.sp,
            fontWeight = FontWeight.Medium,
        )
    }
}
