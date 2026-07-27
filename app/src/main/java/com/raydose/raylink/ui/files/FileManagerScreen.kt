package com.raydose.raylink.ui.files

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckBox
import androidx.compose.material.icons.filled.CheckBoxOutlineBlank
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.raylink.R
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.model.PendingFileTransfer
import com.raydose.raylink.model.SlaveProbeUi
import com.raydose.raylink.ui.components.CompactRadiationHeader
import com.raydose.raylink.ui.labelText
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkAtmosphereBackgroundBrush
import com.raydose.raylink.ui.theme.RaylinkAtmospherePlayerOverlay
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import com.raydose.raylink.ui.theme.ScreenSpec
import java.text.DecimalFormat

@Composable
fun FileManagerScreen(
    viewModel: FileManagerViewModel,
    probes: List<SlaveProbeUi>,
    onRequestUsbAccess: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val state by viewModel.uiState.collectAsState()
    var deleteTarget by remember { mutableStateOf<FileListItem?>(null) }
    var createFolderName by remember { mutableStateOf<String?>(null) }
    var renameTarget by remember { mutableStateOf<FileListItem?>(null) }
    var renameInput by remember { mutableStateOf("") }
    var deleteSelectedPending by remember { mutableStateOf(false) }

    LaunchedEffect(renameTarget?.path) {
        renameInput = renameTarget?.name.orEmpty()
    }

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val summaryHeight = maxHeight * ScreenSpec.SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(RaylinkAtmosphereBackgroundBrush),
        ) {
            CompactRadiationHeader(
                probes = probes,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(summaryHeight),
            )
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(RaylinkAtmosphereBackgroundBrush),
            ) {
                FileManagerToolBar(
                    storage = state.storageLocation,
                    searchQuery = state.searchQuery,
                    message = state.message,
                    currentPath = state.currentPathLabel,
                    pendingTransfer = state.pendingTransfer,
                    requiresUsbAccess = state.requiresUsbAccess,
                    selectionMode = state.selectionMode,
                    selectedCount = state.selectedCount,
                    allVisibleSelected = state.allVisibleSelected,
                    onStorageChange = viewModel::switchStorage,
                    onSearchQueryChange = viewModel::updateSearchQuery,
                    onCreateFolderClick = { createFolderName = "" },
                    onPasteClick = viewModel::pastePendingTransfer,
                    onClearClipboardClick = viewModel::clearPendingTransfer,
                    onRequestUsbAccess = onRequestUsbAccess,
                    onSelectAll = viewModel::selectAllVisible,
                    onClearSelection = viewModel::clearSelection,
                    onExitSelection = viewModel::exitSelectionMode,
                    onCopySelected = viewModel::stageCopySelected,
                    onMoveSelected = viewModel::stageMoveSelected,
                    onDeleteSelected = {
                        if (state.selectedCount > 0) deleteSelectedPending = true
                    },
                    onClose = onBack,
                )
                FileListPanel(
                    items = state.items,
                    isLoading = state.isLoading,
                    searchQuery = state.searchQuery,
                    canGoUp = state.currentPath.isNotBlank() && state.currentPath != state.rootPath,
                    selectionMode = state.selectionMode,
                    selectedPaths = state.selectedPaths,
                    onNavigateUp = viewModel::navigateUp,
                    onOpenDirectory = viewModel::openDirectory,
                    onCopy = viewModel::stageCopy,
                    onMove = viewModel::stageMove,
                    onRename = { item -> renameTarget = item },
                    onDelete = { item -> deleteTarget = item },
                    onLongPressItem = viewModel::beginSelection,
                    onToggleSelection = viewModel::toggleItemSelection,
                    modifier = Modifier
                        .padding(horizontal = 18.dp, vertical = 14.dp)
                        .fillMaxWidth()
                        .weight(1f),
                )
            }
        }
    }

    deleteTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text(stringResource(R.string.dialog_delete_confirm_title), color = RaylinkTextPrimary) },
            text = {
                Text(
                    text = stringResource(R.string.dialog_delete_one, item.name),
                    color = RaylinkTextSecondary,
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        viewModel.deleteItem(item)
                        deleteTarget = null
                    },
                ) { Text(stringResource(R.string.action_delete), color = Color(0xFFFF6B6B)) }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) {
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }

    createFolderName?.let { input ->
        AlertDialog(
            onDismissRequest = { createFolderName = null },
            title = { Text(stringResource(R.string.dialog_new_folder_title), color = RaylinkTextPrimary) },
            text = {
                OutlinedTextField(
                    value = input,
                    onValueChange = { createFolderName = it },
                    singleLine = true,
                    label = { Text(stringResource(R.string.dialog_folder_name_label)) },
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = RaylinkTextPrimary,
                        unfocusedTextColor = RaylinkTextPrimary,
                        focusedLabelColor = RaylinkTextSecondary,
                        unfocusedLabelColor = RaylinkTextSecondary,
                        focusedBorderColor = RaylinkAccentBlue,
                        unfocusedBorderColor = RaylinkTextSecondary,
                    ),
                    modifier = Modifier.fillMaxWidth(),
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        viewModel.createFolder(input)
                        createFolderName = null
                    },
                ) { Text(stringResource(R.string.action_confirm), color = RaylinkAccentBlue) }
            },
            dismissButton = {
                TextButton(onClick = { createFolderName = null }) {
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }

    renameTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { renameTarget = null },
            title = { Text(stringResource(R.string.dialog_rename_title), color = RaylinkTextPrimary) },
            text = {
                OutlinedTextField(
                    value = renameInput,
                    onValueChange = { renameInput = it },
                    singleLine = true,
                    label = { Text(stringResource(R.string.dialog_new_name_label)) },
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = RaylinkTextPrimary,
                        unfocusedTextColor = RaylinkTextPrimary,
                        focusedLabelColor = RaylinkTextSecondary,
                        unfocusedLabelColor = RaylinkTextSecondary,
                        focusedBorderColor = RaylinkAccentBlue,
                        unfocusedBorderColor = RaylinkTextSecondary,
                    ),
                    modifier = Modifier.fillMaxWidth(),
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        viewModel.renameItem(item, renameInput)
                        renameTarget = null
                    },
                ) { Text(stringResource(R.string.action_confirm), color = RaylinkAccentBlue) }
            },
            dismissButton = {
                TextButton(onClick = { renameTarget = null }) {
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }

    if (deleteSelectedPending) {
        AlertDialog(
            onDismissRequest = { deleteSelectedPending = false },
            title = { Text(stringResource(R.string.dialog_batch_delete_title), color = RaylinkTextPrimary) },
            text = {
                Text(
                    text = stringResource(R.string.dialog_batch_delete_message, state.selectedCount),
                    color = RaylinkTextSecondary,
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        viewModel.deleteSelectedItems()
                        deleteSelectedPending = false
                    },
                ) { Text(stringResource(R.string.action_delete), color = Color(0xFFFF6B6B)) }
            },
            dismissButton = {
                TextButton(onClick = { deleteSelectedPending = false }) {
                    Text(stringResource(R.string.action_cancel), color = RaylinkTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun FileManagerToolBar(
    storage: FileStorageLocation,
    searchQuery: String,
    message: String?,
    currentPath: String,
    pendingTransfer: PendingFileTransfer?,
    requiresUsbAccess: Boolean,
    selectionMode: Boolean,
    selectedCount: Int,
    allVisibleSelected: Boolean,
    onStorageChange: (FileStorageLocation) -> Unit,
    onSearchQueryChange: (String) -> Unit,
    onCreateFolderClick: () -> Unit,
    onPasteClick: () -> Unit,
    onClearClipboardClick: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onSelectAll: () -> Unit,
    onClearSelection: () -> Unit,
    onExitSelection: () -> Unit,
    onCopySelected: () -> Unit,
    onMoveSelected: () -> Unit,
    onDeleteSelected: () -> Unit,
    onClose: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(RaylinkAtmospherePlayerOverlay)
            .padding(horizontal = 18.dp, vertical = 10.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = stringResource(R.string.files_title),
                color = RaylinkTextPrimary,
                fontSize = 36.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(modifier = Modifier.width(16.dp))
            StorageTab(
                text = FileStorageLocation.Local.labelText(),
                selected = storage == FileStorageLocation.Local,
                onClick = { onStorageChange(FileStorageLocation.Local) },
            )
            Spacer(modifier = Modifier.width(8.dp))
            StorageTab(
                text = FileStorageLocation.Usb.labelText(),
                selected = storage == FileStorageLocation.Usb,
                onClick = { onStorageChange(FileStorageLocation.Usb) },
            )
            Spacer(modifier = Modifier.width(14.dp))
            OutlinedTextField(
                value = searchQuery,
                onValueChange = onSearchQueryChange,
                placeholder = { Text(stringResource(R.string.files_search_placeholder)) },
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = RaylinkTextPrimary,
                    unfocusedTextColor = RaylinkTextPrimary,
                    focusedPlaceholderColor = RaylinkTextSecondary,
                    unfocusedPlaceholderColor = RaylinkTextSecondary,
                    focusedBorderColor = RaylinkAccentBlue,
                    unfocusedBorderColor = RaylinkTextSecondary,
                ),
                modifier = Modifier.width(260.dp),
            )
            Spacer(modifier = Modifier.width(12.dp))
            if (!selectionMode) {
                ToolButton(text = stringResource(R.string.files_new_folder), onClick = onCreateFolderClick)
            }
            if (pendingTransfer != null && !selectionMode) {
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = stringResource(R.string.action_paste), onClick = onPasteClick)
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = stringResource(R.string.action_clear), onClick = onClearClipboardClick)
            }
            if (storage == FileStorageLocation.Usb && requiresUsbAccess && !selectionMode) {
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = stringResource(R.string.files_grant_usb), onClick = onRequestUsbAccess)
            }
            Spacer(modifier = Modifier.weight(1f))
            Text(
                text = "×",
                color = RaylinkTextPrimary,
                fontSize = 42.sp,
                modifier = Modifier.clickable(onClick = onClose),
            )
        }
        if (selectionMode) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = stringResource(R.string.files_selected_count, selectedCount),
                    color = RaylinkTextPrimary,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Medium,
                )
                Spacer(modifier = Modifier.width(10.dp))
                ToolButton(
                    text = if (allVisibleSelected) {
                        stringResource(R.string.action_deselect_all)
                    } else {
                        stringResource(R.string.action_select_all)
                    },
                    onClick = if (allVisibleSelected) onClearSelection else onSelectAll,
                )
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = stringResource(R.string.action_copy), onClick = onCopySelected)
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = stringResource(R.string.action_move), onClick = onMoveSelected)
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = stringResource(R.string.action_delete), onClick = onDeleteSelected)
                Spacer(modifier = Modifier.weight(1f))
                ToolButton(text = stringResource(R.string.action_done), onClick = onExitSelection)
            }
        }
        Text(
            text = if (currentPath.isBlank()) {
                stringResource(R.string.label_current_path_empty)
            } else {
                stringResource(R.string.label_current_path, currentPath)
            },
            color = RaylinkTextSecondary,
            fontSize = 14.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(top = 8.dp),
        )
        pendingTransfer?.let { transfer ->
            Text(
                text = if (transfer.isMove) {
                    stringResource(R.string.files_pending_move, transfer.sourceName)
                } else {
                    stringResource(R.string.files_pending_copy, transfer.sourceName)
                },
                color = RaylinkTextSecondary,
                fontSize = 14.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
        if (!message.isNullOrBlank()) {
            Text(
                text = message,
                color = RaylinkAccentBlue,
                fontSize = 14.sp,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun FileListPanel(
    items: List<FileListItem>,
    isLoading: Boolean,
    searchQuery: String,
    canGoUp: Boolean,
    selectionMode: Boolean,
    selectedPaths: Set<String>,
    onNavigateUp: () -> Unit,
    onOpenDirectory: (FileListItem) -> Unit,
    onCopy: (FileListItem) -> Unit,
    onMove: (FileListItem) -> Unit,
    onRename: (FileListItem) -> Unit,
    onDelete: (FileListItem) -> Unit,
    onLongPressItem: (FileListItem) -> Unit,
    onToggleSelection: (FileListItem) -> Unit,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(16.dp))
            .background(Color(0x88121726))
            .padding(horizontal = 12.dp, vertical = 10.dp),
    ) {
        if (canGoUp) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(10.dp))
                    .clickable(onClick = onNavigateUp)
                    .padding(horizontal = 8.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(
                    imageVector = Icons.Default.KeyboardArrowUp,
                    contentDescription = stringResource(R.string.action_back_parent),
                    tint = RaylinkTextSecondary,
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(stringResource(R.string.action_back_parent), color = RaylinkTextSecondary, fontSize = 18.sp)
            }
            Spacer(modifier = Modifier.height(8.dp))
        }
        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(stringResource(R.string.files_loading), color = RaylinkTextSecondary, fontSize = 18.sp)
            }
            return
        }
        if (items.isEmpty()) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                val hint = searchQuery.trim().takeIf { it.isNotEmpty() }?.let { q ->
                    stringResource(R.string.files_no_match, q)
                } ?: stringResource(R.string.files_empty)
                Text(hint, color = RaylinkTextSecondary, fontSize = 18.sp)
            }
            return
        }
        LazyColumn(modifier = Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(items, key = { it.path }) { item ->
                FileItemRow(
                    item = item,
                    selectionMode = selectionMode,
                    selected = item.path in selectedPaths,
                    onOpenDirectory = onOpenDirectory,
                    onCopy = onCopy,
                    onMove = onMove,
                    onRename = onRename,
                    onDelete = onDelete,
                    onLongPressItem = onLongPressItem,
                    onToggleSelection = onToggleSelection,
                )
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun FileItemRow(
    item: FileListItem,
    selectionMode: Boolean,
    selected: Boolean,
    onOpenDirectory: (FileListItem) -> Unit,
    onCopy: (FileListItem) -> Unit,
    onMove: (FileListItem) -> Unit,
    onRename: (FileListItem) -> Unit,
    onDelete: (FileListItem) -> Unit,
    onLongPressItem: (FileListItem) -> Unit,
    onToggleSelection: (FileListItem) -> Unit,
) {
    val rowBackground = if (selected) Color(0x883A5A9F) else Color(0x55253143)
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(rowBackground)
            .padding(horizontal = 10.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (selectionMode) {
            Icon(
                imageVector = if (selected) Icons.Default.CheckBox else Icons.Default.CheckBoxOutlineBlank,
                contentDescription = if (selected) {
                    stringResource(R.string.cd_selected)
                } else {
                    stringResource(R.string.cd_not_selected)
                },
                tint = if (selected) RaylinkAccentBlue else RaylinkTextSecondary,
                modifier = Modifier
                    .size(28.dp)
                    .clickable { onToggleSelection(item) },
            )
            Spacer(modifier = Modifier.width(8.dp))
        }
        Icon(
            imageVector = if (item.isDirectory) Icons.Default.Folder else Icons.Default.Description,
            contentDescription = null,
            tint = if (item.isDirectory) Color(0xFFFFD166) else RaylinkTextSecondary,
        )
        Spacer(modifier = Modifier.width(10.dp))
        Column(
            modifier = Modifier
                .weight(1f)
                .combinedClickable(
                    onClick = {
                        if (selectionMode) {
                            onToggleSelection(item)
                        } else if (item.isDirectory) {
                            onOpenDirectory(item)
                        }
                    },
                    onLongClick = { onLongPressItem(item) },
                ),
        ) {
            Text(
                text = item.name,
                color = RaylinkTextPrimary,
                fontSize = 20.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = when {
                    !item.parentPathLabel.isNullOrBlank() && item.isDirectory -> item.parentPathLabel
                    !item.parentPathLabel.isNullOrBlank() ->
                        "${item.parentPathLabel} · ${formatSize(item.sizeBytes)}"
                    item.isDirectory -> stringResource(R.string.files_type_directory)
                    else -> formatSize(item.sizeBytes)
                },
                color = RaylinkTextSecondary,
                fontSize = 14.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        if (!selectionMode) {
            ActionButton(text = stringResource(R.string.action_copy), background = RaylinkAccentBlue, onClick = { onCopy(item) })
            Spacer(modifier = Modifier.width(8.dp))
            ActionButton(text = stringResource(R.string.action_move), background = Color(0xFF4F7EF7), onClick = { onMove(item) })
            Spacer(modifier = Modifier.width(8.dp))
            ActionButton(text = stringResource(R.string.action_rename), background = Color(0xFF556EE6), onClick = { onRename(item) })
            Spacer(modifier = Modifier.width(8.dp))
            ActionButton(text = stringResource(R.string.action_delete), background = Color(0xFFE14B4B), onClick = { onDelete(item) })
        }
    }
}

@Composable
private fun StorageTab(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(12.dp))
            .background(if (selected) Color(0xFF223A66) else Color(0x6615202C))
            .clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 8.dp),
    ) {
        Text(text = text, color = RaylinkTextPrimary, fontSize = 14.sp)
    }
}

@Composable
private fun ToolButton(
    text: String,
    onClick: () -> Unit,
) {
    Button(
        onClick = onClick,
        shape = RoundedCornerShape(16.dp),
        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF212A53)),
    ) {
        Text(text = text, color = RaylinkTextPrimary, fontSize = 16.sp)
    }
}

@Composable
private fun ActionButton(
    text: String,
    background: Color,
    onClick: () -> Unit,
) {
    Button(
        onClick = onClick,
        shape = RoundedCornerShape(999.dp),
        colors = ButtonDefaults.buttonColors(containerColor = background),
        modifier = Modifier.height(34.dp),
        contentPadding = ButtonDefaults.ContentPadding,
    ) {
        Text(text = text, color = RaylinkTextPrimary, fontSize = 14.sp)
    }
}

private fun formatSize(size: Long): String {
    if (size <= 0L) return "0 B"
    val kb = 1024.0
    val mb = kb * 1024.0
    val gb = mb * 1024.0
    val value = size.toDouble()
    val formatter = DecimalFormat("0.##")
    return when {
        value >= gb -> "${formatter.format(value / gb)} GB"
        value >= mb -> "${formatter.format(value / mb)} MB"
        value >= kb -> "${formatter.format(value / kb)} KB"
        else -> "${size} B"
    }
}