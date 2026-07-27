package com.raydose.raylink.ui.music

import android.app.Application
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioManager
import android.media.MediaPlayer
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.raydose.raylink.R
import com.raydose.raylink.data.FileManagerRepository
import com.raydose.raylink.data.MusicRepository
import com.raydose.raylink.model.FileStorageLocation
import com.raydose.raylink.model.MusicPlayMode
import com.raydose.raylink.model.MusicTrack
import com.raydose.raylink.model.MusicUiState
import com.raydose.raylink.ui.tr
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MusicViewModel(application: Application) : AndroidViewModel(application) {
    private val app get() = getApplication<Application>()
    private val repository = MusicRepository(application)
    private val audioManager = application.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var player: MediaPlayer? = null
    private var hasLoadedOnce = false
    private var hasAudioPermission = false

    private val _uiState = MutableStateFlow(MusicUiState(volume = readSystemVolume()))
    val uiState: StateFlow<MusicUiState> = _uiState.asStateFlow()

    init {
        viewModelScope.launch {
            while (true) {
                val mediaPlayer = player
                if (mediaPlayer != null && _uiState.value.isPlaying) {
                    runCatching {
                        _uiState.update {
                            it.copy(
                                positionMillis = mediaPlayer.currentPosition.toLong().coerceAtLeast(0L),
                                durationMillis = mediaPlayer.duration.toLong().coerceAtLeast(0L),
                            )
                        }
                    }
                }
                delay(500L)
            }
        }
    }

    fun loadTracks(hasAudioPermission: Boolean, force: Boolean = false) {
        this.hasAudioPermission = hasAudioPermission
        if (!hasAudioPermission) {
            _uiState.update {
                it.copy(
                    isLoading = false,
                    message = app.tr(R.string.music_need_permission),
                )
            }
            return
        }
        if (hasLoadedOnce && !force) return
        hasLoadedOnce = true
        _uiState.update { it.copy(isLoading = true, message = null) }
        viewModelScope.launch {
            val currentTrackId = _uiState.value.currentTrack?.id
            val tracks = withContext(Dispatchers.IO) {
                repository.loadTracks()
            }
            val restoredIndex = currentTrackId?.let { id ->
                tracks.indexOfFirst { it.id == id }.takeIf { it >= 0 }
            } ?: _uiState.value.currentIndex.coerceInOrDefault(tracks.indices, 0)
            _uiState.update {
                it.copy(
                    tracks = tracks,
                    currentIndex = if (tracks.isEmpty()) -1 else restoredIndex,
                    isLoading = false,
                    message = if (tracks.isEmpty()) app.tr(R.string.music_scan_empty_hint) else null,
                    volume = readSystemVolume(),
                )
            }
        }
    }

    fun updateSearchQuery(query: String) {
        _uiState.update { it.copy(searchQuery = query) }
    }

    fun showImportDialog() {
        _uiState.update { it.copy(showImportDialog = true, message = null) }
    }

    fun dismissImportDialog() {
        if (_uiState.value.isImporting) return
        _uiState.update { it.copy(showImportDialog = false) }
    }

    fun importMusicFiles(
        selections: List<Pair<FileStorageLocation, String>>,
        fileManagerRepository: FileManagerRepository,
    ) {
        if (selections.isEmpty()) return
        viewModelScope.launch {
            _uiState.update { it.copy(isImporting = true, message = null) }
            val currentTrackId = _uiState.value.currentTrack?.id
            val previousIndex = _uiState.value.currentIndex
            val importResult = withContext(Dispatchers.IO) {
                val musicDir = repository.musicImportDirectory().absolutePath
                var successCount = 0
                var failedCount = 0
                selections.forEach { (location, path) ->
                    fileManagerRepository.copyItem(
                        sourceLocation = location,
                        sourcePath = path,
                        targetLocation = FileStorageLocation.Local,
                        targetDirectoryPath = musicDir,
                    ).onSuccess { successCount++ }
                        .onFailure {
                            failedCount++
                            Log.w(TAG, "导入音乐失败 path=$path", it)
                        }
                }
                val tracks = repository.loadTracks()
                val restoredIndex = currentTrackId?.let { id ->
                    tracks.indexOfFirst { it.id == id }.takeIf { it >= 0 }
                } ?: previousIndex.coerceInOrDefault(tracks.indices, 0)
                ImportResult(successCount, failedCount, tracks, restoredIndex)
            }
            hasLoadedOnce = true
            _uiState.update {
                it.copy(
                    tracks = importResult.tracks,
                    currentIndex = if (importResult.tracks.isEmpty()) -1 else importResult.restoredIndex,
                    isLoading = false,
                    isImporting = false,
                    showImportDialog = false,
                    message = when {
                        importResult.failedCount == 0 ->
                            app.tr(R.string.music_import_all_success, importResult.successCount)
                        importResult.successCount == 0 -> app.tr(R.string.music_import_all_failed)
                        else -> app.tr(R.string.music_import_partial, importResult.successCount, importResult.failedCount)
                    },
                )
            }
        }
    }

    fun cyclePlayMode() {
        _uiState.update { it.copy(playMode = it.playMode.next()) }
    }

    fun playTrack(index: Int) {
        val track = _uiState.value.tracks.getOrNull(index) ?: return
        prepareAndPlay(track, index)
    }

    fun playTrackById(trackId: String) {
        val index = _uiState.value.tracks.indexOfFirst { it.id == trackId }
        if (index >= 0) playTrack(index)
    }

    fun togglePlayPause() {
        val current = _uiState.value
        val currentPlayer = player
        when {
            currentPlayer == null && current.currentIndex >= 0 -> playTrack(current.currentIndex)
            currentPlayer == null && current.tracks.isNotEmpty() -> playTrack(0)
            current.isPlaying -> {
                currentPlayer?.pause()
                _uiState.update { it.copy(isPlaying = false) }
            }
            else -> {
                currentPlayer?.start()
                _uiState.update { it.copy(isPlaying = true) }
            }
        }
    }

    fun playPrevious() {
        val tracks = _uiState.value.tracks
        if (tracks.isEmpty()) return
        val index = if (_uiState.value.currentIndex <= 0) tracks.lastIndex else _uiState.value.currentIndex - 1
        playTrack(index)
    }

    fun playNext() {
        val tracks = _uiState.value.tracks
        if (tracks.isEmpty()) return
        playTrack(resolveNextIndex(tracks, _uiState.value.currentIndex, _uiState.value.playMode))
    }

    fun seekToFraction(fraction: Float) {
        val mediaPlayer = player ?: return
        val duration = _uiState.value.durationMillis.takeIf { it > 0 } ?: return
        val target = (duration * fraction.coerceIn(0f, 1f)).toInt()
        runCatching {
            mediaPlayer.seekTo(target)
            _uiState.update { it.copy(positionMillis = target.toLong()) }
        }.onFailure {
            Log.w(TAG, "跳转播放进度失败", it)
        }
    }

    fun setVolume(fraction: Float) {
        val safe = fraction.coerceIn(0f, 1f)
        runCatching {
            val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC).coerceAtLeast(1)
            audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, (safe * max).toInt().coerceIn(0, max), 0)
            _uiState.update { it.copy(volume = readSystemVolume()) }
        }.onFailure {
            _uiState.update { it.copy(message = app.tr(R.string.hint_system_volume_failed)) }
            Log.w(TAG, "系统音量调节失败", it)
        }
    }

    fun stopPlayback() {
        releasePlayer()
        _uiState.update {
            it.copy(isPlaying = false, positionMillis = 0L, durationMillis = it.currentTrack?.durationMillis ?: 0L)
        }
    }

    private fun prepareAndPlay(track: MusicTrack, index: Int) {
        releasePlayer()
        _uiState.update {
            it.copy(
                currentIndex = index,
                isPlaying = false,
                positionMillis = 0L,
                durationMillis = track.durationMillis,
                message = null,
            )
        }
        val context = getApplication<Application>()
        val mediaPlayer = MediaPlayer().apply {
            setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            setOnPreparedListener { prepared ->
                prepared.start()
                _uiState.update {
                    it.copy(
                        isPlaying = true,
                        durationMillis = prepared.duration.toLong().coerceAtLeast(track.durationMillis),
                        volume = readSystemVolume(),
                    )
                }
            }
            setOnCompletionListener {
                handleTrackCompletion()
            }
            setOnErrorListener { _, what, extra ->
                _uiState.update { it.copy(isPlaying = false, message = app.tr(R.string.music_play_failed_code, what, extra)) }
                true
            }
        }
        player = mediaPlayer
        runCatching {
            if (track.uri.scheme == "file") {
                mediaPlayer.setDataSource(requireNotNull(track.uri.path))
            } else {
                mediaPlayer.setDataSource(context, track.uri)
            }
            mediaPlayer.prepareAsync()
        }.onFailure {
            releasePlayer()
            _uiState.update { state ->
                state.copy(isPlaying = false, message = app.tr(R.string.music_cannot_play, track.title))
            }
            Log.w(TAG, "准备播放失败 uri=${track.uri}", it)
        }
    }

    private fun handleTrackCompletion() {
        when (_uiState.value.playMode) {
            MusicPlayMode.SINGLE_LOOP -> {
                val mediaPlayer = player ?: return
                runCatching {
                    mediaPlayer.seekTo(0)
                    mediaPlayer.start()
                    _uiState.update { it.copy(isPlaying = true, positionMillis = 0L) }
                }.onFailure {
                    replayCurrentTrack()
                }
            }
            MusicPlayMode.LIST_LOOP, MusicPlayMode.SHUFFLE -> {
                _uiState.update { state ->
                    state.copy(isPlaying = false, positionMillis = state.durationMillis)
                }
                playNext()
            }
        }
    }

    private fun replayCurrentTrack() {
        val index = _uiState.value.currentIndex
        if (index in _uiState.value.tracks.indices) {
            playTrack(index)
        }
    }

    private fun resolveNextIndex(
        tracks: List<MusicTrack>,
        currentIndex: Int,
        playMode: MusicPlayMode,
    ): Int {
        if (tracks.size == 1) return 0
        return when (playMode) {
            MusicPlayMode.SHUFFLE -> {
                generateSequence { tracks.indices.random() }
                    .first { it != currentIndex }
            }
            MusicPlayMode.LIST_LOOP, MusicPlayMode.SINGLE_LOOP -> {
                if (currentIndex !in tracks.indices || currentIndex == tracks.lastIndex) 0
                else currentIndex + 1
            }
        }
    }

    private fun readSystemVolume(): Float {
        return runCatching {
            val max = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC).coerceAtLeast(1)
            audioManager.getStreamVolume(AudioManager.STREAM_MUSIC).toFloat() / max.toFloat()
        }.getOrDefault(0f)
    }

    private fun releasePlayer() {
        player?.runCatching {
            stop()
            reset()
            release()
        }
        player = null
    }

    override fun onCleared() {
        releasePlayer()
        super.onCleared()
    }

    private fun Int.coerceInOrDefault(range: IntRange, default: Int): Int =
        if (this in range) this else default

    private companion object {
        const val TAG = "RaylinkMusic"
    }
}

private data class ImportResult(
    val successCount: Int,
    val failedCount: Int,
    val tracks: List<MusicTrack>,
    val restoredIndex: Int,
)
