package com.raydose.raylink.ui.standby

import android.graphics.BitmapFactory
import android.net.Uri
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.remember
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import com.raydose.raylink.R
import com.raydose.raylink.data.AlbumImageRepository
import com.raydose.raylink.model.AlbumSettings

/** 待机全屏背景：相册图（需 applyStandby）或 [R.drawable.standby_default]。 */
@Composable
fun StandbyBackground(
    albumSettings: AlbumSettings,
    modifier: Modifier = Modifier,
    scrimAlpha: Float = 0.35f,
) {
    val context = LocalContext.current
    val albumImageRepository = remember { AlbumImageRepository(context) }
    val useAlbumImage = albumSettings.applyStandby &&
        albumSettings.selectedImageUri.isNotBlank() &&
        albumImageRepository.isSelectedImageAvailable(albumSettings.selectedImageUri)

    Box(modifier = modifier.fillMaxSize()) {
        if (useAlbumImage) {
            StandbyAlbumUriImage(
                uri = albumSettings.selectedImageUri,
                modifier = Modifier.fillMaxSize(),
            )
        } else {
            Image(
                painter = painterResource(R.drawable.standby_default),
                contentDescription = null,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize(),
            )
        }
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = scrimAlpha)),
        )
    }
}

@Composable
private fun StandbyAlbumUriImage(
    uri: String,
    modifier: Modifier = Modifier,
) {
    val context = LocalContext.current
    val bitmap = remember(uri) {
        runCatching {
            when {
                uri.startsWith("file:") -> {
                    val path = Uri.parse(uri).path ?: return@runCatching null
                    BitmapFactory.decodeFile(path)?.asImageBitmap()
                }
                uri.startsWith("/") -> BitmapFactory.decodeFile(uri)?.asImageBitmap()
                else -> context.contentResolver.openInputStream(Uri.parse(uri))?.use {
                    BitmapFactory.decodeStream(it)?.asImageBitmap()
                }
            }
        }.getOrNull()
    }
    if (bitmap != null) {
        Image(
            bitmap = bitmap,
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = modifier,
        )
    } else {
        Image(
            painter = painterResource(R.drawable.standby_default),
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = modifier,
        )
    }
}
