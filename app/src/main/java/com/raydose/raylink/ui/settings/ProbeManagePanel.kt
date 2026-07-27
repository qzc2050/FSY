package com.raydose.raylink.ui.settings

import android.widget.Toast
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
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.raydose.raylink.R
import com.raydose.raylink.ui.localizeOtaProgress
import androidx.compose.ui.unit.sp
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.data.NeijiFirmwareRules
import com.raydose.raylink.data.ZjbOtaProgress
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.model.ProbeManageDraft
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkSettingsEditorPanel
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

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
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    onRequestUsbAccess: () -> Unit,
    onUpgradeProbeFirmware: suspend (
        probeId: String,
        fileBytes: ByteArray,
        onProgress: (ZjbOtaProgress) -> Unit,
    ) -> Result<Unit>,
    showSaveSuccess: Boolean = false,
    onDismissSaveSuccess: () -> Unit = {},
    modifier: Modifier = Modifier,
) {
    val pageCount = manageDrafts.size.coerceAtLeast(1)
    val pagerState = rememberPagerState(
        initialPage = selectedProbeIndex.coerceIn(0, pageCount - 1),
        pageCount = { pageCount },
    )
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    val fwNotStarted = stringResource(R.string.probe_fw_status_not_started)
    val fwReading = stringResource(R.string.probe_fw_reading)
    val fwReadFailed = stringResource(R.string.probe_fw_read_failed)
    val fwPushSuccess = stringResource(R.string.probe_fw_push_success)
    val fwUpgradeFailed = stringResource(R.string.probe_fw_upgrade_failed)
    val fwDialogTitle = stringResource(R.string.probe_fw_dialog_title)
    val fwDialogSubtitle = stringResource(R.string.probe_fw_dialog_subtitle)
    val fwConfirmTitle = stringResource(R.string.probe_fw_confirm_title)
    val fwConfirmHint = stringResource(R.string.probe_fw_confirm_hint)

    var showUpdateDialog by remember { mutableStateOf(false) }
    var isUpgrading by remember { mutableStateOf(false) }
    var upgradeProgress by remember { mutableFloatStateOf(0f) }
    var upgradeStatus by remember { mutableStateOf(fwNotStarted) }
    var pendingUpgrade by remember {
        mutableStateOf<Pair<FileStorageLocation, FileListItem>?>(null)
    }
    var upgradeHint by remember { mutableStateOf<String?>(null) }

    fun showMessage(message: String) {
        upgradeHint = message
        Toast.makeText(context.applicationContext, message, Toast.LENGTH_LONG).show()
    }

    fun startUpgrade(location: FileStorageLocation, item: FileListItem) {
        val draft = manageDrafts.getOrNull(selectedProbeIndex) ?: return
        scope.launch {
            isUpgrading = true
            upgradeProgress = 0f
            upgradeStatus = fwReading
            upgradeHint = null
            val bytesResult = withContext(Dispatchers.IO) {
                fileManagerRepository.readNeijiFirmwareBin(location, item.path)
            }
            bytesResult.onFailure { error ->
                isUpgrading = false
                showMessage(error.message ?: fwReadFailed)
                return@launch
            }
            val bytes = bytesResult.getOrThrow()
            onUpgradeProbeFirmware(draft.id, bytes) { progress ->
                upgradeStatus = context.localizeOtaProgress(progress.statusText)
                upgradeProgress = progress.progress
            }.fold(
                onSuccess = {
                    showUpdateDialog = false
                    pendingUpgrade = null
                    showMessage(fwPushSuccess)
                },
                onFailure = { error ->
                    showMessage(error.message ?: fwUpgradeFailed)
                },
            )
            isUpgrading = false
        }
    }

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
                color = RaylinkAccentBlue,
                fontSize = 15.sp,
                modifier = Modifier.padding(horizontal = 28.dp, vertical = 4.dp),
            )
        }
        upgradeHint?.let { hint ->
            Text(
                text = hint,
                color = RaylinkAccentBlue,
                fontSize = 15.sp,
                modifier = Modifier.padding(horizontal = 28.dp, vertical = 4.dp),
            )
        }
        if (isUpgrading) {
            Text(
                text = upgradeStatus,
                color = RaylinkAccentBlue,
                fontSize = 15.sp,
                modifier = Modifier.padding(horizontal = 28.dp, vertical = 2.dp),
            )
            LinearProgressIndicator(
                progress = { upgradeProgress.coerceIn(0f, 1f) },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 28.dp, vertical = 4.dp),
            )
        }

        HorizontalPager(
            state = pagerState,
            userScrollEnabled = !isUpgrading,
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
                onFirmwareUpdateClick = {
                    if (!isUpgrading) {
                        upgradeHint = null
                        showUpdateDialog = true
                    }
                },
                onAddClick = onAddClick,
                onSaveClick = onSaveClick,
            )
        }
    }

    if (showUpdateDialog) {
        ZjbFirmwareUpdateDialog(
            repository = fileManagerRepository,
            usbGrantEpoch = usbGrantEpoch,
            isUpgrading = isUpgrading,
            upgradeProgress = upgradeProgress,
            upgradeStatus = upgradeStatus,
            title = fwDialogTitle,
            subtitle = fwDialogSubtitle,
            onDismiss = {
                if (!isUpgrading) showUpdateDialog = false
            },
            onRequestUsbAccess = onRequestUsbAccess,
            onUpgrade = { location, item ->
                if (!NeijiFirmwareRules.isValidSelection(item.name, item.sizeBytes)) {
                    showMessage(NeijiFirmwareRules.REJECT_MESSAGE)
                    return@ZjbFirmwareUpdateDialog
                }
                pendingUpgrade = location to item
            },
        )
    }

    pendingUpgrade?.let { (location, item) ->
        ZjbFirmwareConfirmDialog(
            fileName = item.name,
            sizeBytes = item.sizeBytes,
            title = fwConfirmTitle,
            hint = fwConfirmHint,
            onDismiss = {
                if (!isUpgrading) pendingUpgrade = null
            },
            onConfirm = {
                pendingUpgrade = null
                startUpgrade(location, item)
            },
        )
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
    onFirmwareUpdateClick: () -> Unit,
    onAddClick: () -> Unit,
    onSaveClick: () -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .clip(RoundedCornerShape(12.dp))
            .background(RaylinkSettingsEditorPanel),
    ) {
        if (!hasDraft || draft == null) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(bottom = CardBottomInset),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    text = stringResource(R.string.probe_manage_empty),
                    color = RaylinkTextSecondary,
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
                onFirmwareUpdateClick = onFirmwareUpdateClick,
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
                    color = RaylinkTextSecondary,
                    fontSize = 20.sp,
                )
            }
            HorizontalDivider(
                modifier = Modifier.padding(
                    top = CardFooterDividerGap,
                    bottom = CardFooterDividerGap,
                ),
                color = RaylinkTextPrimary.copy(alpha = 0.18f),
                thickness = 1.dp,
            )
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                OutlinedButton(onClick = onAddClick) {
                    Text(stringResource(R.string.probe_manage_add), fontSize = 19.sp)
                }
                Button(
                    onClick = onSaveClick,
                    colors = ButtonDefaults.buttonColors(containerColor = RaylinkAccentBlue),
                ) {
                    Text(stringResource(R.string.action_save), fontSize = 19.sp, color = RaylinkTextPrimary)
                }
            }
        }
    }
}
