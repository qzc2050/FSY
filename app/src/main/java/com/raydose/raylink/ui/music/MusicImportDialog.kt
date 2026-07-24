package com.raydose.raylink.ui.music

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
import androidx.compose.material.icons.filled.CheckBox
import androidx.compose.material.icons.filled.CheckBoxOutlineBlank
import androidx.compose.material.icons.filled.Description
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
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.Locale

private val MusicAudioExtensions = setOf("mp3", "wav", "m4a", "aac", "flac", "ogg")

@Composable
fun MusicImportDialog(
    repository: FileManagerRepository,
    usbGrantEpoch: Int,
    isImporting: Boolean,
    onDismiss: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onImport: (List<Pair<FileStorageLocation, String>>) -> Unit,
) {
    var storage by remember { mutableStateOf(FileStorageLocation.Local) }
    var entries by remember { mutableStateOf<List<FileListItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var requiresUsbAccess by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var reloadToken by remember { mutableIntStateOf(0) }
    val pathStack = remember { mutableStateListOf<Pair<String, String>>() }
    val selectedPaths = remember { mutableStateListOf<String>() }

    val currentPath = pathStack.lastOrNull()?.first.orEmpty()
    val currentPathLabel = pathStack.joinToString(" / ") { it.second }

    LaunchedEffect(storage, usbGrantEpoch, reloadToken) {
        isLoading = true
        message = null
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
            if (pathStack.isEmpty() || pathStack.first().first != root.first) {
                pathStack.clear()
                pathStack += root.first to root.second
            }
            val activePath = pathStack.last().first
            entries = withContext(Dispatchers.IO) {
                repository.listItems(storage, activePath, "")
                    .filter { item ->
                        item.isDirectory || item.name.substringAfterLast('.', "")
                            .lowercase(Locale.US) in MusicAudioExtensions
                    }
            }
            requiresUsbAccess = false
        }.onFailure { error ->
            pathStack.clear()
            entries = emptyList()
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
        onDismissRequest = { if (!isImporting) onDismiss() },
        title = {
            Text(
                text = "导入音乐",
                color = RaylinkTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(
                    text = "选择音频文件，将复制到内部 Music 文件夹",
                    color = RaylinkTextSecondary,
                    fontSize = 16.sp,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    ImportStorageTab(
                        text = FileStorageLocation.Local.label,
                        selected = storage == FileStorageLocation.Local,
                        onClick = {
                            if (storage != FileStorageLocation.Local) {
                                storage = FileStorageLocation.Local
                                pathStack.clear()
                                selectedPaths.clear()
                                reloadToken++
                            }
                        },
                    )
                    ImportStorageTab(
                        text = FileStorageLocation.Usb.label,
                        selected = storage == FileStorageLocation.Usb,
                        onClick = {
                            if (storage != FileStorageLocation.Usb) {
                                storage = FileStorageLocation.Usb
                                pathStack.clear()
                                selectedPaths.clear()
                                reloadToken++
                            }
                        },
                    )
                }

                Text(
                    text = if (currentPathLabel.isBlank()) "当前路径：-" else "当前路径：$currentPathLabel",
                    color = RaylinkTextSecondary,
                    fontSize = 16.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )

                if (selectedPaths.isNotEmpty()) {
                    Text(
                        text = "已选 ${selectedPaths.size} 个文件",
                        color = RaylinkAccentBlue,
                        fontSize = 16.sp,
                    )
                }

                message?.let {
                    Text(text = it, color = RaylinkAccentBlue, fontSize = 16.sp)
                }

                if (requiresUsbAccess) {
                    TextButton(onClick = onRequestUsbAccess) {
                        Text("授权 U 盘目录", color = RaylinkTextPrimary, fontSize = 18.sp)
                    }
                } else {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(rowBackground)
                            .clickable(enabled = pathStack.size > 1 && !isLoading && !isImporting) {
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
                            tint = RaylinkTextPrimary,
                        )
                        Text(
                            text = "返回上一级",
                            color = RaylinkTextPrimary,
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
                            CircularProgressIndicator(color = RaylinkAccentBlue)
                        }
                    } else {
                        LazyColumn(
                            modifier = Modifier
                                .fillMaxWidth()
                                .heightIn(max = 320.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(entries, key = { it.path }) { item ->
                                val selected = item.path in selectedPaths
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .clip(RoundedCornerShape(8.dp))
                                        .background(
                                            if (selected) RaylinkAccentBlue.copy(alpha = 0.35f) else rowBackground,
                                        )
                                        .clickable(enabled = !isImporting) {
                                            if (item.isDirectory) {
                                                pathStack += item.path to item.name
                                                reloadToken++
                                            } else if (selected) {
                                                selectedPaths.remove(item.path)
                                            } else {
                                                selectedPaths += item.path
                                            }
                                        }
                                        .padding(horizontal = 12.dp, vertical = 10.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                ) {
                                    Icon(
                                        imageVector = when {
                                            item.isDirectory -> Icons.Default.Folder
                                            selected -> Icons.Default.CheckBox
                                            else -> Icons.Default.CheckBoxOutlineBlank
                                        },
                                        contentDescription = null,
                                        tint = if (item.isDirectory) RaylinkAccentBlue else RaylinkTextPrimary,
                                    )
                                    Text(
                                        text = item.name,
                                        color = RaylinkTextPrimary,
                                        fontSize = 18.sp,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis,
                                        modifier = Modifier.padding(start = 8.dp),
                                    )
                                    if (!item.isDirectory) {
                                        Icon(
                                            imageVector = Icons.Default.Description,
                                            contentDescription = null,
                                            tint = RaylinkTextSecondary,
                                            modifier = Modifier.padding(start = 8.dp),
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            TextButton(
                enabled = selectedPaths.isNotEmpty() && !isImporting && !isLoading,
                onClick = {
                    onImport(selectedPaths.map { storage to it })
                },
            ) {
                if (isImporting) {
                    CircularProgressIndicator(
                        color = RaylinkAccentBlue,
                        modifier = Modifier.padding(end = 8.dp),
                    )
                }
                Text("导入", color = RaylinkTextPrimary, fontSize = 22.sp)
            }
        },
        dismissButton = {
            TextButton(enabled = !isImporting, onClick = onDismiss) {
                Text("取消", color = Color(0xFFD6DCFF), fontSize = 22.sp)
            }
        },
        containerColor = dialogBackground,
        modifier = Modifier.fillMaxWidth(0.62f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}

@Composable
private fun ImportStorageTab(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Text(
        text = text,
        color = if (selected) RaylinkTextPrimary else RaylinkTextSecondary,
        fontSize = 18.sp,
        fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(if (selected) RaylinkAccentBlue else Color.Black.copy(alpha = 0.18f))
            .clickable(onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 8.dp),
    )
}
