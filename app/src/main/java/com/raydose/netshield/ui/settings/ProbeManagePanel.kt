package com.raydose.netshield.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.ProbeManageDraft
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary

private val CardOuterPadding = 12.dp
private val CardFooterPadding = 20.dp
private val CardActionRowHeight = 52.dp
private val CardPaginationRowHeight = 40.dp
private val CardFooterDividerGap = 14.dp
private val CardFooterTopPadding = 12.dp
private val CardBottomInset =
    CardFooterPadding * 2 + CardActionRowHeight + CardPaginationRowHeight + CardFooterDividerGap * 2 + 1.dp
private val CardFooterHeight =
    CardFooterTopPadding + CardPaginationRowHeight + CardFooterDividerGap * 2 + 1.dp +
        CardActionRowHeight + CardFooterPadding
/** 报警音量与「1/1」之间；浮层显示，不挤占表单行 */
private val CardSaveToastStripHeight = 56.dp

@Composable
fun ProbeManagePanel(
    manageDrafts: List<ProbeManageDraft>,
    selectedProbeIndex: Int,
    statusHint: String?,
    onProbePageSelected: (Int) -> Unit,
    onDraftChange: (Int, ProbeManageDraft) -> Unit,
    onVolumeCommitted: (Int) -> Unit,
    onAddClick: () -> Unit,
    onSaveClick: () -> Unit,
    onDataDetailClick: (Int) -> Unit,
    onRemoveProbe: (Int) -> Unit,
    showSaveSuccess: Boolean = false,
    onDismissSaveSuccess: () -> Unit = {},
    modifier: Modifier = Modifier,
) {
    val pageCount = manageDrafts.size.coerceAtLeast(1)
    val pagerState = rememberPagerState(
        initialPage = selectedProbeIndex.coerceIn(0, pageCount - 1),
        pageCount = { pageCount },
    )

    LaunchedEffect(selectedProbeIndex, pageCount) {
        val target = selectedProbeIndex.coerceIn(0, pageCount - 1)
        if (pagerState.currentPage != target) {
            pagerState.animateScrollToPage(target)
        }
    }

    LaunchedEffect(pagerState.currentPage) {
        if (pagerState.currentPage != selectedProbeIndex) {
            onProbePageSelected(pagerState.currentPage)
        }
    }

    val pageLabel = if (manageDrafts.isEmpty()) {
        "0/0"
    } else {
        "${pagerState.currentPage + 1}/${manageDrafts.size}"
    }

    Column(modifier = modifier.fillMaxSize()) {
        if (statusHint != null) {
            Text(
                text = statusHint,
                color = NetShieldAccentBlue,
                fontSize = 15.sp,
                modifier = Modifier.padding(horizontal = 28.dp, vertical = 4.dp),
            )
        }

        HorizontalPager(
            state = pagerState,
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(horizontal = CardOuterPadding, vertical = 8.dp),
        ) { page ->
            ProbeManageCard(
                pageLabel = pageLabel,
                hasDraft = page in manageDrafts.indices,
                draft = manageDrafts.getOrNull(page),
                showSaveSuccess = showSaveSuccess && page == selectedProbeIndex,
                onDismissSaveSuccess = onDismissSaveSuccess,
                onDraftChange = { onDraftChange(page, it) },
                onVolumeCommitted = { onVolumeCommitted(page) },
                onDataDetailClick = { onDataDetailClick(page) },
                onDeleteClick = { onRemoveProbe(page) },
                onAddClick = onAddClick,
                onSaveClick = onSaveClick,
            )
        }
    }
}

@Composable
private fun ProbeManageCard(
    pageLabel: String,
    hasDraft: Boolean,
    draft: ProbeManageDraft?,
    showSaveSuccess: Boolean,
    onDismissSaveSuccess: () -> Unit,
    onDraftChange: (ProbeManageDraft) -> Unit,
    onVolumeCommitted: () -> Unit,
    onDataDetailClick: () -> Unit,
    onDeleteClick: () -> Unit,
    onAddClick: () -> Unit,
    onSaveClick: () -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .clip(RoundedCornerShape(12.dp))
            .background(NetShieldSettingsEditorPanel),
    ) {
        if (!hasDraft || draft == null) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(bottom = CardBottomInset),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = "暂无探头，请点击左下角「添加探头」",
                    color = NetShieldTextSecondary,
                    fontSize = 18.sp,
                )
            }
        } else {
            ProbeManageEditorPage(
                draft = draft,
                onDraftChange = onDraftChange,
                onVolumeCommitted = onVolumeCommitted,
                onDataDetailClick = onDataDetailClick,
                onDeleteClick = onDeleteClick,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(
                        start = 16.dp,
                        top = 12.dp,
                        end = 16.dp,
                        bottom = CardBottomInset,
                    ),
            )
        }

        if (showSaveSuccess) {
            Box(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .padding(bottom = CardFooterHeight)
                    .height(CardSaveToastStripHeight),
                contentAlignment = Alignment.Center,
            ) {
                SaveSuccessToast(onDismiss = onDismissSaveSuccess)
            }
        }

        Column(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .padding(horizontal = CardFooterPadding)
                .padding(top = CardFooterTopPadding, bottom = CardFooterPadding),
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(CardPaginationRowHeight),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = pageLabel,
                    color = NetShieldTextSecondary,
                    fontSize = 20.sp,
                )
            }
            HorizontalDivider(
                modifier = Modifier.padding(
                    top = CardFooterDividerGap,
                    bottom = CardFooterDividerGap,
                ),
                color = NetShieldTextPrimary.copy(alpha = 0.18f),
                thickness = 1.dp,
            )
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedButton(onClick = onAddClick) {
                    Text("+ 添加探头", fontSize = 19.sp)
                }
                Button(
                    onClick = onSaveClick,
                    colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
                ) {
                    Text("保存", fontSize = 19.sp, color = NetShieldTextPrimary)
                }
            }
        }
    }
}
