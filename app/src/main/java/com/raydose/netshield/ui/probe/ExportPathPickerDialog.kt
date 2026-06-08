package com.raydose.netshield.ui.probe

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
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
import androidx.compose.ui.window.DialogProperties
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

@Composable
fun ExportPathPickerDialog(
    repository: FileManagerRepository,
    usbGrantEpoch: Int,
    onDismiss: () -> Unit,
    onConfirm: (FileStorageLocation, String) -> Unit,
) {
    var storage by remember { mutableStateOf(FileStorageLocation.Local) }
    var directories by remember { mutableStateOf<List<FileListItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var requiresUsbAccess by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var reloadToken by remember { mutableIntStateOf(0) }
    val pathStack = remember { mutableStateListOf<Pair<String, String>>() }

    val currentPath = pathStack.lastOrNull()?.first.orEmpty()
    val currentPathLabel = pathStack.joinToString(" / ") { it.second }

    LaunchedEffect(storage, usbGrantEpoch, reloadToken) {
        isLoading = true
        message = null
        runCatching {
            val root = withContext(Dispatchers.IO) { repository.resolveRoot(storage) }
            if (root == null) {
                pathStack.clear()
                directories = emptyList()
                requiresUsbAccess = storage == FileStorageLocation.Usb
                message = if (storage == FileStorageLocation.Usb) {
                    "未检测到 U 盘，请插入后重试"
                } else {
                    "未检测到可用存储"
                }
                return@runCatching
            }
            if (pathStack.isEmpty() || pathStack.first().first != root.first) {
                pathStack.clear()
                pathStack += root.first to root.second
            }
            val activePath = pathStack.last().first
            directories = withContext(Dispatchers.IO) {
                repository.listDirectoryEntries(storage, activePath)
            }
            requiresUsbAccess = false
        }.onFailure { error ->
            pathStack.clear()
            directories = emptyList()
            requiresUsbAccess = storage == FileStorageLocation.Usb
            message = when {
                storage == FileStorageLocation.Usb -> "U 盘不可用：${error.message ?: "请插入后重试"}"
                else -> error.message ?: "读取目录失败"
            }
        }
        isLoading = false
    }

    val dialogBackground = Color(0xFF3946A1)
    val rowBackground = Color(0xFF4452B8)

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = "选择导出路径",
                color = NetShieldTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    StorageTab(
                        text = FileStorageLocation.Local.label,
                        selected = storage == FileStorageLocation.Local,
                        onClick = {
                            if (storage != FileStorageLocation.Local) {
                                storage = FileStorageLocation.Local
                                pathStack.clear()
                                reloadToken++
                            }
                        },
                    )
                    StorageTab(
                        text = FileStorageLocation.Usb.label,
                        selected = storage == FileStorageLocation.Usb,
                        onClick = {
                            if (storage != FileStorageLocation.Usb) {
                                storage = FileStorageLocation.Usb
                                pathStack.clear()
                                reloadToken++
                            }
                        },
                    )
                }

                Text(
                    text = if (currentPathLabel.isBlank()) "当前路径：-" else "当前路径：$currentPathLabel",
                    color = NetShieldTextSecondary,
                    fontSize = 18.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )

                message?.let {
                    Text(text = it, color = NetShieldAccentBlue, fontSize = 16.sp)
                }

                if (!requiresUsbAccess) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(rowBackground)
                            .clickable(enabled = pathStack.size > 1 && !isLoading) {
                                if (pathStack.size > 1) {
                                    pathStack.removeAt(pathStack.lastIndex)
                                    reloadToken++
                                }
                            }
                            .padding(horizontal = 12.dp, vertical = 10.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Icon(
                            imageVector = Icons.Default.KeyboardArrowUp,
                            contentDescription = null,
                            tint = NetShieldTextPrimary,
                        )
                        Text(
                            text = "返回上一级",
                            color = NetShieldTextPrimary,
                            fontSize = 18.sp,
                            modifier = Modifier.padding(start = 6.dp),
                        )
                    }

                    if (isLoading) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 24.dp),
                            horizontalArrangement = Arrangement.Center,
                        ) {
                            CircularProgressIndicator(color = NetShieldAccentBlue)
                        }
                    } else {
                        LazyColumn(
                            modifier = Modifier
                                .fillMaxWidth()
                                .heightIn(max = 280.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(directories, key = { it.path }) { item ->
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .clip(RoundedCornerShape(8.dp))
                                        .background(rowBackground)
                                        .clickable {
                                            pathStack += item.path to item.name
                                            reloadToken++
                                        }
                                        .padding(horizontal = 12.dp, vertical = 10.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Folder,
                                        contentDescription = null,
                                        tint = NetShieldAccentBlue,
                                    )
                                    Text(
                                        text = item.name,
                                        color = NetShieldTextPrimary,
                                        fontSize = 18.sp,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                        modifier = Modifier.padding(start = 8.dp),
                                    )
                                }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                enabled = currentPath.isNotBlank() && !requiresUsbAccess && !isLoading,
                onClick = { onConfirm(storage, currentPath) },
            ) {
                Text("确定", color = NetShieldTextPrimary, fontSize = 22.sp)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("取消", color = Color(0xFFD6DCFF), fontSize = 22.sp)
            }
        },
        containerColor = dialogBackground,
        modifier = Modifier.fillMaxWidth(0.56f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}

@Composable
private fun StorageTab(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Text(
        text = text,
        color = if (selected) NetShieldTextPrimary else NetShieldTextSecondary,
        fontSize = 18.sp,
        fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) NetShieldAccentBlue else Color.Black.copy(alpha = 0.18f))
            .clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 8.dp),
    )
}
