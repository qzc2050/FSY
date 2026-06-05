package com.raydose.netshield.ui.music

import android.app.Application
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioManager
import android.media.MediaPlayer
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.raydose.netshield.data.MusicRepository
import com.raydose.netshield.model.MusicTrack
import com.raydose.netshield.model.MusicUiState
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MusicViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = MusicRepository(application)
    private val audioManager = application.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var player: MediaPlayer? = null
    private var hasLoadedOnce = false

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
        if (!hasAudioPermission) {
            _uiState.update {
                it.copy(
                    isLoading = false,
                    message = "需要授予音乐读取权限后才能扫描本机和 U 盘音乐",
                )
            }
            return
        }
        if (hasLoadedOnce && !force) return
        hasLoadedOnce = true
        _uiState.update { it.copy(isLoading = true, message = null) }
        viewModelScope.launch {
            val tracks = withContext(Dispatchers.IO) {
                repository.loadTracks()
            }
            _uiState.update {
                it.copy(
                    tracks = tracks,
                    currentIndex = if (tracks.isEmpty()) -1 else it.currentIndex.coerceInOrDefault(tracks.indices, 0),
                    isLoading = false,
                    message = if (tracks.isEmpty()) "未找到音乐文件，请将音频放入 Music 目录或插入 U 盘" else null,
                    volume = readSystemVolume(),
                )
            }
        }
    }

    fun playTrack(index: Int) {
        val track = _uiState.value.tracks.getOrNull(index) ?: return
        prepareAndPlay(track, index)
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
        val index = if (_uiState.value.currentIndex !in tracks.indices || _uiState.value.currentIndex == tracks.lastIndex) {
            0
        } else {
            _uiState.value.currentIndex + 1
        }
        playTrack(index)
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
            _uiState.update { it.copy(message = "系统音量调节失败") }
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
                _uiState.update { state ->
                    state.copy(isPlaying = false, positionMillis = state.durationMillis)
                }
                playNext()
            }
            setOnErrorListener { _, what, extra ->
                _uiState.update { it.copy(isPlaying = false, message = "音乐播放失败：$what/$extra") }
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
                state.copy(isPlaying = false, message = "无法播放：${track.title}")
            }
            Log.w(TAG, "准备播放失败 uri=${track.uri}", it)
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
        const val TAG = "NetShieldMusic"
    }
}
