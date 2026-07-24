package com.raydose.raylink.ui.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.model.ProbeManageDraft
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary

/** 设置页首行：探头名（上行）+ 剂量数据（下行），左对齐与导航标题一致 */
@Composable
fun SettingsTopProbeStatus(
    drafts: List<ProbeManageDraft>,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .fillMaxSize()
            .padding(
                start = SettingsLayout.navContentStart,
                end = 56.dp,
            ),
        horizontalArrangement = Arrangement.spacedBy(
            SettingsLayout.probeSummaryColumnSpacing,
            Alignment.Start,
        ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (drafts.isEmpty()) {
            ProbeSummaryColumn(
                name = "—",
                value = "---",
                online = false,
            )
        } else {
            drafts.forEach { draft ->
                ProbeSummaryColumn(
                    name = draft.displayName,
                    value = if (draft.isTcpOnline) draft.doseRateSummary else "---",
                    online = draft.isTcpOnline,
                )
            }
        }
    }
}

@Composable
private fun ProbeSummaryColumn(
    name: String,
    value: String,
    online: Boolean,
) {
    Column(
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
            textAlign = TextAlign.Start,
        )
        Spacer(modifier = Modifier.height(SettingsLayout.probeSummaryNameDoseGap))
        Row(verticalAlignment = Alignment.Bottom) {
            Text(
                text = value,
                color = if (online) RaylinkTextPrimary else RaylinkTextSecondary,
                fontSize = 34.sp,
                lineHeight = 38.sp,
            )
            Text(
                text = " μSv/h",
                color = RaylinkTextSecondary,
                fontSize = 17.sp,
                modifier = Modifier.padding(bottom = 4.dp),
            )
        }
    }
}
