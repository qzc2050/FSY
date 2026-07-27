package com.raydose.raylink.ui.album

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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.DialogProperties
import com.raydose.raylink.R
import com.raydose.raylink.data.AlbumImageRepository
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.data.isAlbumImageFile
import com.raydose.raylink.model.FileListItem
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.ui.labelText
import com.raydose.raylink.ui.tr
import com.raydose.raylink.ui.theme.RaylinkAccentBlue
import com.raydose.raylink.ui.theme.RaylinkTextPrimary
import com.raydose.raylink.ui.theme.RaylinkTextSecondary
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

    val context = LocalContext.current

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
                    context.tr(R.string.storage_usb_not_found)
                } else {
                    context.tr(R.string.storage_not_available)
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
                storage == FileStorageLocation.Usb ->
                    context.tr(R.string.storage_usb_unavailable, error.message ?: context.tr(R.string.storage_usb_not_found))
                else -> error.message ?: context.tr(R.string.storage_read_dir_failed)
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
                text = stringResource(R.string.album_picker_title),
                color = RaylinkTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text(
                    text = stringResource(R.string.album_picker_hint),
                    color = RaylinkTextSecondary,
                    fontSize = 16.sp,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    AlbumPickerStorageTab(
                        text = FileStorageLocation.Local.labelText(),
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
                        text = FileStorageLocation.Usb.labelText(),
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
                    text = if (currentPathLabel.isBlank()) {
                        stringResource(R.string.label_current_path_empty)
                    } else {
                        stringResource(R.string.label_current_path, currentPathLabel)
                    },
                    color = RaylinkTextSecondary,
                    fontSize = 16.sp,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                message?.let {
                    Text(text = it, color = RaylinkAccentBlue, fontSize = 16.sp)
                }
                if (requiresUsbAccess) {
                    TextButton(onClick = onRequestUsbAccess) {
                        Text(stringResource(R.string.storage_grant_usb), color = RaylinkTextPrimary, fontSize = 18.sp)
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
                            text = stringResource(R.string.action_back_parent),
                            color = RaylinkTextPrimary,
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
                            CircularProgressIndicator(color = RaylinkAccentBlue)
                        }
                    } else if (entries.isEmpty()) {
                        Text(
                            text = stringResource(R.string.album_no_images_in_dir),
                            color = RaylinkTextSecondary,
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
                Text(stringResource(R.string.action_cancel), color = Color(0xFFD6DCFF), fontSize = 22.sp)
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
            tint = RaylinkAccentBlue,
            modifier = Modifier.size(36.dp),
        )
        Text(
            text = name,
            color = RaylinkTextPrimary,
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
                tint = RaylinkTextSecondary,
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
