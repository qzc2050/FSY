package com.raydose.netshield.ui.components

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.basicMarquee
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldMessageBar
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec

@Composable
fun MessageTickerBar(
    messageIndex: Int,
    messageTextAt: (Int) -> String,
    messageCount: Int,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    widthFraction: Float = 0.54f,
    scrollEnabled: Boolean = false,
    animateMessageChange: Boolean = false,
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
        Box(
            modifier = Modifier
                .weight(1f)
                .height(26.dp)
                .padding(horizontal = 12.dp)
                .clip(RectangleShape),
            contentAlignment = Alignment.CenterStart,
        ) {
            if (animateMessageChange) {
                AnimatedContent(
                    targetState = messageIndex,
                    transitionSpec = {
                        slideInVertically(
                            animationSpec = tween(ScreenSpec.MESSAGE_TICKER_SLIDE_MS),
                            initialOffsetY = { height -> height },
                        ) togetherWith slideOutVertically(
                            animationSpec = tween(ScreenSpec.MESSAGE_TICKER_SLIDE_MS),
                            targetOffsetY = { height -> -height },
                        )
                    },
                    label = "messageTickerVerticalScroll",
                ) { index ->
                    MessageTickerText(
                        text = messageTextAt(index),
                        useHorizontalMarquee = false,
                    )
                }
            } else {
                MessageTickerText(
                    text = messageTextAt(messageIndex),
                    useHorizontalMarquee = scrollEnabled,
                )
            }
        }
        Text(text = "$messageCount", color = NetShieldTextSecondary, fontSize = 18.sp)
        Text(text = "  ▲", color = NetShieldTextSecondary, fontSize = 16.sp)
    }
}

@Composable
private fun MessageTickerText(
    text: String,
    useHorizontalMarquee: Boolean,
    modifier: Modifier = Modifier,
) {
    Text(
        text = text,
        color = NetShieldTextPrimary,
        fontSize = 18.sp,
        maxLines = 1,
        overflow = if (useHorizontalMarquee) TextOverflow.Visible else TextOverflow.Ellipsis,
        modifier = modifier.then(
            if (useHorizontalMarquee) {
                Modifier.basicMarquee(
                    iterations = Int.MAX_VALUE,
                    repeatDelayMillis = ScreenSpec.MESSAGE_TICKER_MARQUEE_DELAY_MS,
                    initialDelayMillis = ScreenSpec.MESSAGE_TICKER_MARQUEE_DELAY_MS,
                )
            } else {
                Modifier
            },
        ),
    )
}
