package com.raydose.raylink.model

import android.net.Uri
import java.util.Locale

enum class MusicPlayMode(val label: String) {
    LIST_LOOP("列表循环"),
    SINGLE_LOOP("单曲循环"),
    SHUFFLE("随机播放");

    fun next(): MusicPlayMode = entries[(ordinal + 1) % entries.size]
}

data class MusicTrack(
    val id: String,
    val title: String,
    val artist: String,
    val uri: Uri,
    val durationMillis: Long = 0L,
    val sourceLabel: String = "",
    /** 用于 MediaStore / 文件扫描去重的绝对路径 */
    val filePath: String? = null,
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
    val searchQuery: String = "",
    val playMode: MusicPlayMode = MusicPlayMode.LIST_LOOP,
    val showImportDialog: Boolean = false,
    val isImporting: Boolean = false,
) {
    val currentTrack: MusicTrack?
        get() = tracks.getOrNull(currentIndex)

    /** 搜索命中置顶，其余保持原顺序 */
    val displayTracks: List<MusicTrack>
        get() {
            val keyword = searchQuery.trim().lowercase(Locale.getDefault())
            if (keyword.isEmpty()) return tracks
            val (matched, rest) = tracks.partition { it.matchesKeyword(keyword) }
            return matched + rest
        }

    fun isSearchMatch(track: MusicTrack): Boolean {
        val keyword = searchQuery.trim().lowercase(Locale.getDefault())
        if (keyword.isEmpty()) return false
        return track.matchesKeyword(keyword)
    }
}

private fun MusicTrack.matchesKeyword(keyword: String): Boolean =
    title.lowercase(Locale.getDefault()).contains(keyword) ||
        artist.lowercase(Locale.getDefault()).contains(keyword) ||
        sourceLabel.lowercase(Locale.getDefault()).contains(keyword)
