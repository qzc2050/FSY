package com.raydose.netshield.ui.files

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.model.PendingFileTransfer
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.CompactRadiationHeader
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldAtmosphereBackgroundBrush
import com.raydose.netshield.ui.theme.NetShieldAtmospherePlayerOverlay
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec
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

    LaunchedEffect(renameTarget?.path) {
        renameInput = renameTarget?.name.orEmpty()
    }

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val summaryHeight = maxHeight * ScreenSpec.SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(NetShieldAtmosphereBackgroundBrush),
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
                    .background(NetShieldAtmosphereBackgroundBrush),
            ) {
                FileManagerToolBar(
                    storage = state.storageLocation,
                    searchQuery = state.searchQuery,
                    message = state.message,
                    currentPath = state.currentPathLabel,
                    pendingTransfer = state.pendingTransfer,
                    requiresUsbAccess = state.requiresUsbAccess,
                    onStorageChange = viewModel::switchStorage,
                    onSearchQueryChange = viewModel::updateSearchQuery,
                    onCreateFolderClick = { createFolderName = "" },
                    onPasteClick = viewModel::pastePendingTransfer,
                    onClearClipboardClick = viewModel::clearPendingTransfer,
                    onRequestUsbAccess = onRequestUsbAccess,
                    onClose = onBack,
                )
                FileListPanel(
                    items = state.items,
                    isLoading = state.isLoading,
                    canGoUp = state.currentPath.isNotBlank() && state.currentPath != state.rootPath,
                    onNavigateUp = viewModel::navigateUp,
                    onOpenDirectory = viewModel::openDirectory,
                    onCopy = viewModel::stageCopy,
                    onMove = viewModel::stageMove,
                    onRename = { item -> renameTarget = item },
                    onDelete = { item -> deleteTarget = item },
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
            title = { Text("删除确认", color = NetShieldTextPrimary) },
            text = {
                Text(
                    text = "确定删除 ${item.name} 吗？",
                    color = NetShieldTextSecondary,
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        viewModel.deleteItem(item)
                        deleteTarget = null
                    },
                ) { Text("删除", color = Color(0xFFFF6B6B)) }
            },
            dismissButton = {
                TextButton(onClick = { deleteTarget = null }) {
                    Text("取消", color = NetShieldTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }

    createFolderName?.let { input ->
        AlertDialog(
            onDismissRequest = { createFolderName = null },
            title = { Text("新建文件夹", color = NetShieldTextPrimary) },
            text = {
                OutlinedTextField(
                    value = input,
                    onValueChange = { createFolderName = it },
                    singleLine = true,
                    label = { Text("文件夹名称") },
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = NetShieldTextPrimary,
                        unfocusedTextColor = NetShieldTextPrimary,
                        focusedLabelColor = NetShieldTextSecondary,
                        unfocusedLabelColor = NetShieldTextSecondary,
                        focusedBorderColor = NetShieldAccentBlue,
                        unfocusedBorderColor = NetShieldTextSecondary,
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
                ) { Text("确定", color = NetShieldAccentBlue) }
            },
            dismissButton = {
                TextButton(onClick = { createFolderName = null }) {
                    Text("取消", color = NetShieldTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }

    renameTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { renameTarget = null },
            title = { Text("重命名", color = NetShieldTextPrimary) },
            text = {
                OutlinedTextField(
                    value = renameInput,
                    onValueChange = { renameInput = it },
                    singleLine = true,
                    label = { Text("新名称") },
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = NetShieldTextPrimary,
                        unfocusedTextColor = NetShieldTextPrimary,
                        focusedLabelColor = NetShieldTextSecondary,
                        unfocusedLabelColor = NetShieldTextSecondary,
                        focusedBorderColor = NetShieldAccentBlue,
                        unfocusedBorderColor = NetShieldTextSecondary,
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
                ) { Text("确定", color = NetShieldAccentBlue) }
            },
            dismissButton = {
                TextButton(onClick = { renameTarget = null }) {
                    Text("取消", color = NetShieldTextSecondary)
                }
            },
            containerColor = Color(0xFF1B2233),
        )
    }
}

@Composable
private fun FileManagerToolBar(
    storage: FileStorageLocation,
    searchQuery: String,
    message: String?,
    currentPath: String,
    pendingTransfer: PendingFileTransfer?,
    requiresUsbAccess: Boolean,
    onStorageChange: (FileStorageLocation) -> Unit,
    onSearchQueryChange: (String) -> Unit,
    onCreateFolderClick: () -> Unit,
    onPasteClick: () -> Unit,
    onClearClipboardClick: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onClose: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(NetShieldAtmospherePlayerOverlay)
            .padding(horizontal = 18.dp, vertical = 10.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = "文件管理",
                color = NetShieldTextPrimary,
                fontSize = 36.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(modifier = Modifier.width(16.dp))
            StorageTab(
                text = FileStorageLocation.Local.label,
                selected = storage == FileStorageLocation.Local,
                onClick = { onStorageChange(FileStorageLocation.Local) },
            )
            Spacer(modifier = Modifier.width(8.dp))
            StorageTab(
                text = FileStorageLocation.Usb.label,
                selected = storage == FileStorageLocation.Usb,
                onClick = { onStorageChange(FileStorageLocation.Usb) },
            )
            Spacer(modifier = Modifier.width(14.dp))
            OutlinedTextField(
                value = searchQuery,
                onValueChange = onSearchQueryChange,
                placeholder = { Text("搜索") },
                singleLine = true,
                colors = OutlinedTextFieldDefaults.colors(
                    focusedTextColor = NetShieldTextPrimary,
                    unfocusedTextColor = NetShieldTextPrimary,
                    focusedPlaceholderColor = NetShieldTextSecondary,
                    unfocusedPlaceholderColor = NetShieldTextSecondary,
                    focusedBorderColor = NetShieldAccentBlue,
                    unfocusedBorderColor = NetShieldTextSecondary,
                ),
                modifier = Modifier.width(260.dp),
            )
            Spacer(modifier = Modifier.width(12.dp))
            ToolButton(text = "新建文件夹", onClick = onCreateFolderClick)
            if (pendingTransfer != null) {
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = "粘贴", onClick = onPasteClick)
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = "清空", onClick = onClearClipboardClick)
            }
            if (storage == FileStorageLocation.Usb && requiresUsbAccess) {
                Spacer(modifier = Modifier.width(8.dp))
                ToolButton(text = "授权U盘", onClick = onRequestUsbAccess)
            }
            Spacer(modifier = Modifier.weight(1f))
            Text(
                text = "×",
                color = NetShieldTextPrimary,
                fontSize = 42.sp,
                modifier = Modifier.clickable(onClick = onClose),
            )
        }
        Text(
            text = if (currentPath.isBlank()) "当前路径：-" else "当前路径：$currentPath",
            color = NetShieldTextSecondary,
            fontSize = 14.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(top = 8.dp),
        )
        pendingTransfer?.let { transfer ->
            Text(
                text = if (transfer.isMove) {
                    "待移动：${transfer.sourceName}"
                } else {
                    "待复制：${transfer.sourceName}"
                },
                color = NetShieldTextSecondary,
                fontSize = 14.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
        if (!message.isNullOrBlank()) {
            Text(
                text = message,
                color = NetShieldAccentBlue,
                fontSize = 14.sp,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
    }
}

@Composable
private fun FileListPanel(
    items: List<FileListItem>,
    isLoading: Boolean,
    canGoUp: Boolean,
    onNavigateUp: () -> Unit,
    onOpenDirectory: (FileListItem) -> Unit,
    onCopy: (FileListItem) -> Unit,
    onMove: (FileListItem) -> Unit,
    onRename: (FileListItem) -> Unit,
    onDelete: (FileListItem) -> Unit,
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
                    contentDescription = "返回上一级",
                    tint = NetShieldTextSecondary,
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text("返回上一级", color = NetShieldTextSecondary, fontSize = 18.sp)
            }
            Spacer(modifier = Modifier.height(8.dp))
        }
        if (isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("加载中...", color = NetShieldTextSecondary, fontSize = 18.sp)
            }
            return
        }
        if (items.isEmpty()) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("暂无文件", color = NetShieldTextSecondary, fontSize = 18.sp)
            }
            return
        }
        LazyColumn(modifier = Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(items, key = { it.path }) { item ->
                FileItemRow(
                    item = item,
                    onOpenDirectory = onOpenDirectory,
                    onCopy = onCopy,
                    onMove = onMove,
                    onRename = onRename,
                    onDelete = onDelete,
                )
            }
        }
    }
}

@Composable
private fun FileItemRow(
    item: FileListItem,
    onOpenDirectory: (FileListItem) -> Unit,
    onCopy: (FileListItem) -> Unit,
    onMove: (FileListItem) -> Unit,
    onRename: (FileListItem) -> Unit,
    onDelete: (FileListItem) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(Color(0x55253143))
            .padding(horizontal = 10.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            imageVector = if (item.isDirectory) Icons.Default.Folder else Icons.Default.Description,
            contentDescription = null,
            tint = if (item.isDirectory) Color(0xFFFFD166) else NetShieldTextSecondary,
        )
        Spacer(modifier = Modifier.width(10.dp))
        Column(
            modifier = Modifier
                .weight(1f)
                .clickable(enabled = item.isDirectory) {
                    if (item.isDirectory) onOpenDirectory(item)
                },
        ) {
            Text(
                text = item.name,
                color = NetShieldTextPrimary,
                fontSize = 20.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = if (item.isDirectory) "目录" else "${formatSize(item.sizeBytes)}",
                color = NetShieldTextSecondary,
                fontSize = 14.sp,
            )
        }
        ActionButton(text = "复制", background = NetShieldAccentBlue, onClick = { onCopy(item) })
        Spacer(modifier = Modifier.width(8.dp))
        ActionButton(text = "移动", background = Color(0xFF4F7EF7), onClick = { onMove(item) })
        Spacer(modifier = Modifier.width(8.dp))
        ActionButton(text = "重命名", background = Color(0xFF556EE6), onClick = { onRename(item) })
        Spacer(modifier = Modifier.width(8.dp))
        ActionButton(text = "删除", background = Color(0xFFE14B4B), onClick = { onDelete(item) })
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
        Text(text = text, color = NetShieldTextPrimary, fontSize = 14.sp)
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
        Text(text = text, color = NetShieldTextPrimary, fontSize = 16.sp)
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
        Text(text = text, color = NetShieldTextPrimary, fontSize = 14.sp)
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