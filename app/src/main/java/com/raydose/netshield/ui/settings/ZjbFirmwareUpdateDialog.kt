package com.raydose.netshield.ui.settings

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
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
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
import androidx.compose.ui.window.Dialog
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

@Composable
fun ZjbFirmwareUpdateDialog(
    repository: FileManagerRepository,
    usbGrantEpoch: Int,
    isUpgrading: Boolean,
    upgradeProgress: Float,
    upgradeStatus: String,
    onDismiss: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onUpgrade: (FileStorageLocation, FileListItem) -> Unit,
    title: String = "转接板固件更新",
    subtitle: String = "请选择转接板固件文件。升级完成后转接板将自动重启。",
) {
    var storage by remember { mutableStateOf(FileStorageLocation.Usb) }
    var entries by remember { mutableStateOf<List<FileListItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var requiresUsbAccess by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var reloadToken by remember { mutableIntStateOf(0) }
    val pathStack = remember { mutableStateListOf<Pair<String, String>>() }
    var selectedPath by remember { mutableStateOf<String?>(null) }

    val currentPathLabel = pathStack.joinToString(" / ") { it.second }
    val rowBackground = Color.White.copy(alpha = 0.08f)

    LaunchedEffect(storage, usbGrantEpoch, reloadToken) {
        isLoading = true
        message = null
        selectedPath = null
        runCatching {
            val root = withContext(Dispatchers.IO) { repository.resolveRoot(storage) }
            if (root == null) {
                pathStack.clear()
                entries = emptyList()
                requiresUsbAccess = storage == FileStorageLocation.Usb
                message = if (storage == FileStorageLocation.Usb) {
                    "未检测到 U 盘，请插入后重试"
                } else {
                    "未检测到可用存储"
                }
                return@runCatching
            }
            requiresUsbAccess = false
            if (pathStack.isEmpty() || pathStack.first().first != root.first) {
                pathStack.clear()
                pathStack += root.first to root.second
            }
            val activePath = pathStack.last().first
            entries = withContext(Dispatchers.IO) {
                repository.listItems(storage, activePath, "")
            }
        }.onFailure { error ->
            message = error.message ?: "读取目录失败"
            entries = emptyList()
        }
        isLoading = false
    }

    Dialog(onDismissRequest = { if (!isUpgrading) onDismiss() }) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.82f)
                .clip(RoundedCornerShape(12.dp))
                .background(NetShieldSettingsEditorPanel)
                .padding(20.dp),
        ) {
            Text(
                text = title,
                color = NetShieldTextPrimary,
                fontSize = 24.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = subtitle,
                color = NetShieldTextSecondary,
                fontSize = 16.sp,
                modifier = Modifier.padding(top = 8.dp),
            )

            if (isUpgrading) {
                Text(
                    text = upgradeStatus,
                    color = NetShieldAccentBlue,
                    fontSize = 16.sp,
                    modifier = Modifier.padding(top = 12.dp),
                )
                LinearProgressIndicator(
                    progress = { upgradeProgress.coerceIn(0f, 1f) },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp),
                )
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 16.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                ZjbStorageTab(
                    text = FileStorageLocation.Local.label,
                    selected = storage == FileStorageLocation.Local,
                    onClick = {
                        if (!isUpgrading && storage != FileStorageLocation.Local) {
                            storage = FileStorageLocation.Local
                            pathStack.clear()
                            reloadToken++
                        }
                    },
                )
                ZjbStorageTab(
                    text = FileStorageLocation.Usb.label,
                    selected = storage == FileStorageLocation.Usb,
                    onClick = {
                        if (!isUpgrading && storage != FileStorageLocation.Usb) {
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
                fontSize = 16.sp,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(top = 12.dp),
            )

            selectedPath?.let {
                Text(
                    text = "已选：${entries.firstOrNull { item -> item.path == it }?.name ?: "固件.bin"}",
                    color = NetShieldAccentBlue,
                    fontSize = 16.sp,
                    modifier = Modifier.padding(top = 6.dp),
                )
            }

            message?.let {
                Text(text = it, color = NetShieldAccentBlue, fontSize = 16.sp, modifier = Modifier.padding(top = 6.dp))
            }

            if (!isUpgrading) {
                if (requiresUsbAccess) {
                    TextButton(onClick = onRequestUsbAccess) {
                        Text("授权 U 盘目录", color = NetShieldTextPrimary, fontSize = 18.sp)
                    }
                } else {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 8.dp)
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
                        Icon(Icons.Default.KeyboardArrowUp, contentDescription = null, tint = NetShieldTextPrimary)
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
                                .heightIn(max = 320.dp)
                                .padding(top = 8.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(entries, key = { it.path }) { item ->
                                val selected = selectedPath == item.path
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .clip(RoundedCornerShape(8.dp))
                                        .background(
                                            if (selected) NetShieldAccentBlue.copy(alpha = 0.25f) else rowBackground,
                                        )
                                        .clickable {
                                            if (item.isDirectory) {
                                                pathStack += item.path to item.name
                                                reloadToken++
                                            } else {
                                                selectedPath = item.path
                                            }
                                        }
                                        .padding(horizontal = 12.dp, vertical = 10.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    Icon(
                                        imageVector = if (item.isDirectory) Icons.Default.Folder else Icons.Default.Description,
                                        contentDescription = null,
                                        tint = if (selected) NetShieldAccentBlue else NetShieldTextPrimary,
                                    )
                                    Text(
                                        text = item.name,
                                        color = NetShieldTextPrimary,
                                        fontSize = 18.sp,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                        modifier = Modifier
                                            .weight(1f)
                                            .padding(start = 10.dp),
                                    )
                                }
                            }
                        }
                    }
                }
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 18.dp),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = onDismiss, enabled = !isUpgrading) {
                    Text("取消", color = NetShieldTextPrimary, fontSize = 18.sp)
                }
                TextButton(
                    onClick = {
                        val item = entries.firstOrNull { it.path == selectedPath && !it.isDirectory }
                            ?: return@TextButton
                        onUpgrade(storage, item)
                    },
                    enabled = !isUpgrading && !selectedPath.isNullOrBlank(),
                ) {
                    Text(
                        text = if (isUpgrading) "升级中…" else "开始升级",
                        color = NetShieldAccentBlue,
                        fontSize = 18.sp,
                    )
                }
            }
        }
    }
}

@Composable
private fun ZjbStorageTab(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Text(
        text = text,
        color = if (selected) NetShieldAccentBlue else NetShieldTextSecondary,
        fontSize = 18.sp,
        fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp, vertical = 8.dp),
    )
}
