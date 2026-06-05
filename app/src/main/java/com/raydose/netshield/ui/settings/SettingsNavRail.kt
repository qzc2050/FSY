package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

@Composable
fun SettingsNavRail(
    selectedTab: SettingsTab,
    onTabSelected: (SettingsTab) -> Unit,
    navRailWidth: Dp,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .width(navRailWidth)
            .fillMaxHeight()
            .padding(
                start = SettingsLayout.navRailStartPadding,
                top = 16.dp,
                end = 12.dp,
                bottom = 16.dp,
            ),
    ) {
        val mainTabs = SettingsTab.entries.filter { it != SettingsTab.About }
        mainTabs.forEach { tab ->
            SettingsNavItem(
                tab = tab,
                selected = tab == selectedTab,
                onClick = { onTabSelected(tab) },
            )
        }
        Spacer(modifier = Modifier.weight(1f))
        SettingsNavItem(
            tab = SettingsTab.About,
            selected = selectedTab == SettingsTab.About,
            onClick = { onTabSelected(SettingsTab.About) },
        )
    }
}

@Composable
private fun SettingsNavItem(
    tab: SettingsTab,
    selected: Boolean,
    onClick: () -> Unit,
) {
    val contentColor = if (selected) NetShieldTextPrimary else NetShieldTextSecondary
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(
                if (selected) NetShieldAccentBlue.copy(alpha = 0.35f)
                else NetShieldTextPrimary.copy(alpha = 0f),
            )
            .clickable(onClick = onClick)
            .padding(
                horizontal = SettingsLayout.navItemInnerPadding,
                vertical = 10.dp,
            ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            imageVector = tab.icon,
            contentDescription = tab.label,
            tint = contentColor,
            modifier = Modifier.size(SettingsLayout.navIconSize),
        )
        Spacer(modifier = Modifier.size(SettingsLayout.navIconTextGap))
        Text(
            text = tab.label,
            color = contentColor,
            fontSize = if (selected) 20.sp else 18.sp,
        )
    }
}
