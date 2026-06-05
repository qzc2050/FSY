package com.raydose.netshield.model

import android.net.Uri

data class MusicTrack(
    val id: String,
    val title: String,
    val artist: String,
    val uri: Uri,
    val durationMillis: Long = 0L,
    val sourceLabel: String = "",
)

data class MusicUiState(
    val tracks: List<MusicTrack> = emptyList(),
    val currentIndex: Int = -1,
    val isPlaying: Boolean = false,
    val positionMillis: Long = 0L,
    val durationMillis: Long = 0L,
    val volume: Float = 0f,
    val isLoading: Boolean = false,
    val message: String? = null,
) {
    val currentTrack: MusicTrack?
        get() = tracks.getOrNull(currentIndex)
}
