package com.raydose.netshield.ui.music

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
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
import androidx.compose.material.icons.filled.MusicNote
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Repeat
import androidx.compose.material.icons.filled.RepeatOne
import androidx.compose.material.icons.filled.Shuffle
import androidx.compose.material.icons.filled.SkipNext
import androidx.compose.material.icons.filled.SkipPrevious
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.Image
import com.raydose.netshield.R
import com.raydose.netshield.data.FileManagerRepository
import com.raydose.netshield.model.MusicPlayMode
import com.raydose.netshield.model.MusicTrack
import com.raydose.netshield.model.MusicUiState
import com.raydose.netshield.model.SlaveProbeUi
import com.raydose.netshield.ui.components.CompactRadiationHeader
import com.raydose.netshield.ui.theme.NetShieldAccentBlue
import com.raydose.netshield.ui.theme.NetShieldAtmosphereBackgroundBrush
import com.raydose.netshield.ui.theme.NetShieldAtmosphereListPanelBg
import com.raydose.netshield.ui.theme.NetShieldAtmospherePlayerOverlay
import com.raydose.netshield.ui.theme.NetShieldSettingsEditorPanel
import com.raydose.netshield.ui.theme.NetShieldTextPrimary
import com.raydose.netshield.ui.theme.NetShieldTextSecondary
import com.raydose.netshield.ui.theme.ScreenSpec

@Composable
fun MusicScreen(
    viewModel: MusicViewModel,
    fileManagerRepository: FileManagerRepository,
    usbGrantEpoch: Int,
    hasAudioPermission: Boolean,
    probes: List<SlaveProbeUi>,
    onRequestPermission: () -> Unit,
    onRequestUsbAccess: () -> Unit,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val state by viewModel.uiState.collectAsState()

    LaunchedEffect(hasAudioPermission) {
        viewModel.loadTracks(hasAudioPermission)
    }

    if (state.showImportDialog) {
        MusicImportDialog(
            repository = fileManagerRepository,
            usbGrantEpoch = usbGrantEpoch,
            isImporting = state.isImporting,
            onDismiss = viewModel::dismissImportDialog,
            onRequestUsbAccess = onRequestUsbAccess,
            onImport = { selections ->
                viewModel.importMusicFiles(selections, fileManagerRepository)
            },
        )
    }

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val summaryHeight = maxHeight * ScreenSpec.SETTINGS_PROBE_SUMMARY_HEIGHT_FRACTION
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(NetShieldAtmosphereBackgroundBrush)
        ) {
            CompactRadiationHeader(
                probes = probes,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(summaryHeight),
            )
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(NetShieldAtmosphereBackgroundBrush),
            ) {
                if (!hasAudioPermission) {
                    PermissionPanel(
                        onRequestPermission = onRequestPermission,
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(28.dp),
                    )
                } else {
                    MusicContent(
                        state = state,
                        onSearchQueryChange = viewModel::updateSearchQuery,
                        onImportClick = viewModel::showImportDialog,
                        onTrackClick = viewModel::playTrackById,
                        onCyclePlayMode = viewModel::cyclePlayMode,
                        onPrevious = viewModel::playPrevious,
                        onToggle = viewModel::togglePlayPause,
                        onNext = viewModel::playNext,
                        onClose = {
                            viewModel.stopPlayback()
                            onBack()
                        },
                        modifier = Modifier.fillMaxSize(),
                    )
                }
            }
        }
    }
}

@Composable
private fun PermissionPanel(
    onRequestPermission: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(24.dp))
            .background(Color.Black.copy(alpha = 0.24f)),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                text = "需要音乐读取权限",
                color = NetShieldTextPrimary,
                fontSize = 28.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = "授权后可扫描内部 Music 目录和 U 盘 Music/music 目录",
                color = NetShieldTextSecondary,
                fontSize = 18.sp,
                modifier = Modifier.padding(top = 12.dp, bottom = 24.dp),
            )
            Button(
                onClick = onRequestPermission,
                colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
            ) {
                Text("授权并扫描", color = NetShieldTextPrimary, fontSize = 18.sp)
            }
        }
    }
}

@Composable
private fun MusicContent(
    state: MusicUiState,
    onSearchQueryChange: (String) -> Unit,
    onImportClick: () -> Unit,
    onTrackClick: (String) -> Unit,
    onCyclePlayMode: () -> Unit,
    onPrevious: () -> Unit,
    onToggle: () -> Unit,
    onNext: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
) {
    BoxWithConstraints(modifier = modifier.background(NetShieldAtmosphereBackgroundBrush)) {
        val windowBarHeight = maxHeight * 0.15f
        Column(modifier = Modifier.fillMaxSize()) {
            MusicWindowBar(
                onMinimize = onClose,
                onClose = onClose,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(windowBarHeight),
            )
            Row(modifier = Modifier.weight(1f)) {
                TrackListPanel(
                    state = state,
                    onSearchQueryChange = onSearchQueryChange,
                    onImportClick = onImportClick,
                    onTrackClick = onTrackClick,
                    modifier = Modifier
                        .weight(0.2f)
                        .fillMaxHeight(),
                )
                PlayerPanel(
                    state = state,
                    onCyclePlayMode = onCyclePlayMode,
                    onPrevious = onPrevious,
                    onToggle = onToggle,
                    onNext = onNext,
                    modifier = Modifier
                        .weight(0.8f)
                        .fillMaxHeight(),
                )
            }
        }
    }
}

@Composable
private fun MusicWindowBar(
    onMinimize: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            modifier = Modifier
                .weight(0.2f)
                .fillMaxHeight()
                .background(NetShieldAtmosphereListPanelBg)
                .padding(start = 24.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(
                text = "音乐",
                color = NetShieldTextPrimary,
                fontSize = 30.sp,
                fontWeight = FontWeight.SemiBold,
            )
        }
        Box(
            modifier = Modifier
                .weight(0.8f)
                .fillMaxHeight()
                .background(NetShieldAtmospherePlayerOverlay)
                .padding(end = 24.dp),
            contentAlignment = Alignment.CenterEnd,
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(28.dp)) {
                Text(
                    text = "－",
                    color = NetShieldTextPrimary,
                    fontSize = 34.sp,
                    modifier = Modifier.clickable(onClick = onMinimize),
                )
                Text(
                    text = "×",
                    color = NetShieldTextPrimary,
                    fontSize = 36.sp,
                    modifier = Modifier.clickable(onClick = onClose),
                )
            }
        }
    }
}

@Composable
private fun TrackListPanel(
    state: MusicUiState,
    onSearchQueryChange: (String) -> Unit,
    onImportClick: () -> Unit,
    onTrackClick: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val displayTracks = state.displayTracks
    Column(
        modifier = modifier
            .background(NetShieldAtmosphereListPanelBg)
            .padding(horizontal = 16.dp, vertical = 18.dp),
    ) {
        Text(
            text = "歌曲列表",
            color = NetShieldTextPrimary,
            fontSize = 22.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Text(
            text = "路径：内部 /Music，U盘 /Music / music",
            color = NetShieldTextSecondary,
            fontSize = 12.sp,
            modifier = Modifier.padding(top = 8.dp),
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(modifier = Modifier.height(12.dp))
        OutlinedTextField(
            value = state.searchQuery,
            onValueChange = onSearchQueryChange,
            placeholder = { Text("搜索歌曲", fontSize = 14.sp) },
            singleLine = true,
            colors = OutlinedTextFieldDefaults.colors(
                focusedTextColor = NetShieldTextPrimary,
                unfocusedTextColor = NetShieldTextPrimary,
                focusedPlaceholderColor = NetShieldTextSecondary,
                unfocusedPlaceholderColor = NetShieldTextSecondary,
                focusedBorderColor = NetShieldAccentBlue,
                unfocusedBorderColor = NetShieldTextSecondary,
            ),
            modifier = Modifier.fillMaxWidth(),
        )
        Spacer(modifier = Modifier.height(10.dp))
        Button(
            onClick = onImportClick,
            enabled = !state.isImporting,
            colors = ButtonDefaults.buttonColors(containerColor = NetShieldAccentBlue),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(
                text = if (state.isImporting) "导入中…" else "导入",
                color = NetShieldTextPrimary,
                fontSize = 16.sp,
            )
        }
        Spacer(modifier = Modifier.height(12.dp))
        if (state.isLoading) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(text = "扫描中…", color = NetShieldTextSecondary, fontSize = 16.sp)
            }
        } else if (displayTracks.isEmpty()) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(
                    text = state.message ?: "未找到音乐文件",
                    color = NetShieldTextSecondary,
                    fontSize = 16.sp,
                )
            }
        } else {
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(displayTracks, key = { it.id }) { track ->
                    TrackRow(
                        track = track,
                        selected = track.id == state.currentTrack?.id,
                        searchPinned = state.isSearchMatch(track),
                        onClick = { onTrackClick(track.id) },
                    )
                }
            }
        }
    }
}

@Composable
private fun TrackRow(
    track: MusicTrack,
    selected: Boolean,
    searchPinned: Boolean,
    onClick: () -> Unit,
) {
    val bg = when {
        selected -> NetShieldAccentBlue.copy(alpha = 0.28f)
        searchPinned -> NetShieldAccentBlue.copy(alpha = 0.14f)
        else -> NetShieldSettingsEditorPanel.copy(alpha = 0.82f)
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(bg)
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = track.title,
                color = NetShieldTextPrimary,
                fontSize = 16.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = "${track.artist} · ${track.sourceLabel.ifBlank { "本地音乐" }}",
                color = NetShieldTextSecondary,
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        Text(
            text = formatDuration(track.durationMillis),
            color = NetShieldTextSecondary,
            fontSize = 14.sp,
        )
    }
}

@Composable
private fun PlayerPanel(
    state: MusicUiState,
    onCyclePlayMode: () -> Unit,
    onPrevious: () -> Unit,
    onToggle: () -> Unit,
    onNext: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val current = state.currentTrack
    Column(
        modifier = modifier
            .background(NetShieldAtmospherePlayerOverlay)
            .padding(horizontal = 42.dp, vertical = 28.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        GalaxyCover(
            modifier = Modifier
                .fillMaxWidth(0.6f)
                .aspectRatio(16f / 9f),
        )
        Spacer(modifier = Modifier.height(24.dp))
        Row(
            modifier = Modifier.fillMaxWidth(0.6f),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                imageVector = Icons.Filled.MusicNote,
                contentDescription = null,
                tint = NetShieldTextPrimary,
                modifier = Modifier.size(30.dp),
            )
            Text(
                text = current?.title ?: "未选择音乐",
                color = NetShieldTextPrimary,
                fontSize = 20.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(start = 12.dp),
            )
        }
        Spacer(modifier = Modifier.height(38.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            PlayModeButton(
                playMode = state.playMode,
                onClick = onCyclePlayMode,
                enabled = state.tracks.isNotEmpty(),
            )
            Spacer(modifier = Modifier.width(58.dp))
            PlayerIconButton(onClick = onPrevious, enabled = state.tracks.isNotEmpty()) {
                Icon(Icons.Filled.SkipPrevious, contentDescription = "上一首")
            }
            Spacer(modifier = Modifier.width(58.dp))
            IconButton(
                onClick = onToggle,
                enabled = state.tracks.isNotEmpty(),
                modifier = Modifier
                    .size(96.dp)
                    .clip(RoundedCornerShape(48.dp))
                    .background(NetShieldAccentBlue),
            ) {
                Icon(
                    imageVector = if (state.isPlaying) Icons.Filled.Pause else Icons.Filled.PlayArrow,
                    contentDescription = if (state.isPlaying) "暂停" else "播放",
                    tint = NetShieldTextPrimary,
                    modifier = Modifier.size(56.dp),
                )
            }
            Spacer(modifier = Modifier.width(58.dp))
            PlayerIconButton(onClick = onNext, enabled = state.tracks.isNotEmpty()) {
                Icon(Icons.Filled.SkipNext, contentDescription = "下一首")
            }
        }
        state.message?.let {
            Text(
                text = it,
                color = NetShieldTextSecondary,
                fontSize = 16.sp,
                modifier = Modifier.padding(top = 24.dp),
            )
        }
    }
}

@Composable
private fun PlayModeButton(
    playMode: MusicPlayMode,
    onClick: () -> Unit,
    enabled: Boolean,
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        PlayerIconButton(onClick = onClick, enabled = enabled) {
            Icon(
                imageVector = playMode.icon(),
                contentDescription = playMode.label,
            )
        }
        Text(
            text = playMode.label,
            color = if (enabled) NetShieldTextSecondary else NetShieldTextSecondary.copy(alpha = 0.5f),
            fontSize = 12.sp,
            modifier = Modifier.padding(top = 4.dp),
        )
    }
}

private fun MusicPlayMode.icon(): ImageVector = when (this) {
    MusicPlayMode.LIST_LOOP -> Icons.Filled.Repeat
    MusicPlayMode.SINGLE_LOOP -> Icons.Filled.RepeatOne
    MusicPlayMode.SHUFFLE -> Icons.Filled.Shuffle
}

@Composable
private fun GalaxyCover(modifier: Modifier = Modifier) {
    Image(
        painter = painterResource(R.drawable.music_galaxy_cover),
        contentDescription = "音乐封面",
        contentScale = ContentScale.Crop,
        modifier = modifier
            .clip(RoundedCornerShape(12.dp)),
    )
}

@Composable
private fun PlayerIconButton(
    onClick: () -> Unit,
    enabled: Boolean,
    icon: @Composable () -> Unit,
) {
    IconButton(
        onClick = onClick,
        enabled = enabled,
        modifier = Modifier.size(76.dp),
    ) {
        Box(
            modifier = Modifier
                .size(68.dp)
                .clip(RoundedCornerShape(34.dp))
                .background(Color.White.copy(alpha = if (enabled) 0.16f else 0.06f)),
            contentAlignment = Alignment.Center,
        ) {
            androidx.compose.material3.ProvideTextStyle(
                value = androidx.compose.ui.text.TextStyle(color = NetShieldTextPrimary),
            ) {
                icon()
            }
        }
    }
}

private fun formatDuration(millis: Long): String {
    if (millis <= 0L) return "--:--"
    val totalSeconds = millis / 1000L
    val minutes = totalSeconds / 60L
    val seconds = totalSeconds % 60L
    return "%02d:%02d".format(minutes, seconds)
}
