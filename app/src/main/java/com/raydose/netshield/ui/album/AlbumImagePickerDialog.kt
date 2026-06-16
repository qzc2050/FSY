package com.raydose.netshield.ui.album

import android.content.Context
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Image
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
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.DialogProperties
import com.raydose.netshield.data.AlbumImageRepository
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.data.isAlbumImageFile
import com.raydose.netshield.model.FileListItem
import com.raydose.netshield.model.FileStorageLocation
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

@Composable
fun AlbumImagePickerDialog(
    repository: FileManagerRepository,
    albumImageRepository: AlbumImageRepository,
    usbGrantEpoch: Int,
    isImporting: Boolean,
    initialStorage: FileStorageLocation = FileStorageLocation.Local,
    initialDirectoryPath: String = "",
    onDismiss: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onImageSelected: (FileStorageLocation, String, String) -> Unit,
) {
    var storage by remember { mutableStateOf(initialStorage) }
    var entries by remember { mutableStateOf<List<FileListItem>>(emptyList()) }
    var isLoading by remember { mutableStateOf(true) }
    var requiresUsbAccess by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var reloadToken by remember { mutableIntStateOf(0) }
    val pathStack = remember { mutableStateListOf<Pair<String, String>>() }
    var hasAppliedInitialPath by remember(initialStorage, initialDirectoryPath) { mutableStateOf(false) }

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
                if (!hasAppliedInitialPath) {
                    val preferred = when {
                        initialDirectoryPath.isNotBlank() -> initialDirectoryPath
                        storage == FileStorageLocation.Local ->
                            albumImageRepository.defaultLocalPicturesDirectory().absolutePath
                        else -> root.first
                    }
                    val browsePath = albumImageRepository.resolveInitialBrowsePath(root.first, preferred)
                    pathStack += albumImageRepository.buildPathStack(root.first, root.second, browsePath)
                    hasAppliedInitialPath = true
                } else {
                    pathStack += root.first to root.second
                }
            }
            val activePath = pathStack.last().first
            entries = withContext(Dispatchers.IO) {
                repository.listItems(storage, activePath, "")
                    .filter { item -> item.isDirectory || isAlbumImageFile(item.name) }
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
                text = "选择图片",
                color = NetShieldTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(
                    text = "在本应用内浏览图片，不调用系统相册",
                    color = NetShieldTextSecondary,
                    fontSize = 16.sp,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    AlbumPickerStorageTab(
                        text = FileStorageLocation.Local.label,
                        selected = storage == FileStorageLocation.Local,
                        onClick = {
                            if (storage != FileStorageLocation.Local) {
                                storage = FileStorageLocation.Local
                                pathStack.clear()
                                hasAppliedInitialPath = false
                                reloadToken++
                            }
                        },
                    )
                    AlbumPickerStorageTab(
                        text = FileStorageLocation.Usb.label,
                        selected = storage == FileStorageLocation.Usb,
                        onClick = {
                            if (storage != FileStorageLocation.Usb) {
                                storage = FileStorageLocation.Usb
                                pathStack.clear()
                                hasAppliedInitialPath = false
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
                )
                message?.let {
                    Text(text = it, color = NetShieldAccentBlue, fontSize = 16.sp)
                }
                if (requiresUsbAccess) {
                    TextButton(onClick = onRequestUsbAccess) {
                        Text("授权 U 盘目录", color = NetShieldTextPrimary, fontSize = 18.sp)
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
                            tint = NetShieldTextPrimary,
                        )
                        Text(
                            text = "返回上一级",
                            color = NetShieldTextPrimary,
                            fontSize = 18.sp,
                            modifier = Modifier.padding(start = 6.dp),
                        )
                    }
                    if (isLoading || isImporting) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 24.dp),
                            horizontalArrangement = Arrangement.Center,
                        ) {
                            CircularProgressIndicator(color = NetShieldAccentBlue)
                        }
                    } else if (entries.isEmpty()) {
                        Text(
                            text = "当前目录没有图片",
                            color = NetShieldTextSecondary,
                            fontSize = 16.sp,
                            modifier = Modifier.padding(vertical = 24.dp),
                        )
                    } else {
                        LazyVerticalGrid(
                            columns = GridCells.Adaptive(minSize = 108.dp),
                            modifier = Modifier
                                .fillMaxWidth()
                                .heightIn(max = 360.dp),
                            contentPadding = PaddingValues(4.dp),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(entries, key = { it.path }) { item ->
                                if (item.isDirectory) {
                                    AlbumFolderTile(
                                        name = item.name,
                                        background = rowBackground,
                                        onClick = {
                                            pathStack += item.path to item.name
                                            reloadToken++
                                        },
                                    )
                                } else {
                                    AlbumImageTile(
                                        item = item,
                                        storage = storage,
                                        onClick = {
                                            onImageSelected(storage, item.path, pathStack.lastOrNull()?.first.orEmpty())
                                        },
                                    )
                                }
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(enabled = !isImporting, onClick = onDismiss) {
                Text("取消", color = Color(0xFFD6DCFF), fontSize = 22.sp)
            }
        },
        containerColor = dialogBackground,
        modifier = Modifier.fillMaxWidth(0.68f),
        properties = DialogProperties(usePlatformDefaultWidth = false),
    )
}

@Composable
private fun AlbumFolderTile(
    name: String,
    background: Color,
    onClick: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1f)
            .clip(RoundedCornerShape(8.dp))
            .background(background)
            .clickable(onClick = onClick)
            .padding(8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Icon(
            imageVector = Icons.Default.Folder,
            contentDescription = null,
            tint = NetShieldAccentBlue,
            modifier = Modifier.size(36.dp),
        )
        Text(
            text = name,
            color = NetShieldTextPrimary,
            fontSize = 14.sp,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(top = 6.dp),
        )
    }
}

@Composable
private fun AlbumImageTile(
    item: FileListItem,
    storage: FileStorageLocation,
    onClick: () -> Unit,
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1f)
            .clip(RoundedCornerShape(8.dp))
            .background(Color(0xFF4452B8))
            .clickable(onClick = onClick),
    ) {
        val context = LocalContext.current
        val thumbnail by produceState<ImageBitmap?>(null, item.path) {
            value = withContext(Dispatchers.IO) {
                decodeAlbumThumbnail(context, item, storage)?.asImageBitmap()
            }
        }
        if (thumbnail != null) {
            Image(
                bitmap = thumbnail!!,
                contentDescription = item.name,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize(),
            )
        } else {
            Icon(
                imageVector = Icons.Default.Image,
                contentDescription = null,
                tint = NetShieldTextSecondary,
                modifier = Modifier
                    .align(Alignment.Center)
                    .size(32.dp),
            )
        }
    }
}

private fun decodeAlbumThumbnail(
    context: Context,
    item: FileListItem,
    storage: FileStorageLocation,
): android.graphics.Bitmap? {
    return runCatching {
        when {
            item.path.startsWith("/storage/") || storage == FileStorageLocation.Local -> {
                decodeFileThumbnail(File(item.path))
            }
            item.path.startsWith("content:") -> {
                context.contentResolver.openInputStream(Uri.parse(item.path))?.use { input ->
                    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
                    BitmapFactory.decodeStream(input, null, bounds)
                    val sampleSize = (bounds.outWidth / 256).coerceAtLeast(1)
                    context.contentResolver.openInputStream(Uri.parse(item.path))?.use { stream ->
                        BitmapFactory.decodeStream(
                            stream,
                            null,
                            BitmapFactory.Options().apply { inSampleSize = sampleSize },
                        )
                    }
                }
            }
            else -> null
        }
    }.getOrNull()
}

private fun decodeFileThumbnail(file: File): android.graphics.Bitmap? {
    if (!file.isFile || !file.canRead()) return null
    val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(file.absolutePath, options)
    if (options.outWidth <= 0 || options.outHeight <= 0) return null
    val sampleSize = (options.outWidth / 256).coerceAtLeast(1)
    val decodeOptions = BitmapFactory.Options().apply { inSampleSize = sampleSize }
    return BitmapFactory.decodeFile(file.absolutePath, decodeOptions)
}

@Composable
private fun AlbumPickerStorageTab(
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
