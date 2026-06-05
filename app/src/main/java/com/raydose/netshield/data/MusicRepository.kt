package com.raydose.netshield.data

import android.content.ContentUris
import android.content.Context
import android.provider.MediaStore
import android.util.Log
import com.raydose.netshield.model.MusicTrack
import java.io.File
import java.util.Locale

class MusicRepository(context: Context) {
    private val appContext = context.applicationContext

    fun loadTracks(): List<MusicTrack> {
        val tracks = linkedMapOf<String, MusicTrack>()
        loadMediaStoreTracks().forEach { tracks[it.id] = it }
        loadStorageMusicTracks().forEach { track ->
            tracks.putIfAbsent(track.uri.toString(), track)
        }
        return tracks.values.sortedWith(
            compareBy<MusicTrack> { it.sourceLabel }
                .thenBy { it.title.lowercase(Locale.getDefault()) },
        )
    }

    private fun loadMediaStoreTracks(): List<MusicTrack> {
        val uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI
        val projection = arrayOf(
            MediaStore.Audio.Media._ID,
            MediaStore.Audio.Media.TITLE,
            MediaStore.Audio.Media.ARTIST,
            MediaStore.Audio.Media.DURATION,
            MediaStore.Audio.Media.RELATIVE_PATH,
        )
        val selection = "${MediaStore.Audio.Media.IS_MUSIC} != 0"
        val sortOrder = "${MediaStore.Audio.Media.TITLE} COLLATE NOCASE ASC"
        return runCatching {
            appContext.contentResolver.query(uri, projection, selection, null, sortOrder)?.use { cursor ->
                val idColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID)
                val titleColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE)
                val artistColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST)
                val durationColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DURATION)
                val relativePathColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.RELATIVE_PATH)
                buildList {
                    while (cursor.moveToNext()) {
                        val mediaId = cursor.getLong(idColumn)
                        val contentUri = ContentUris.withAppendedId(uri, mediaId)
                        val title = cursor.getString(titleColumn).orEmpty().ifBlank { "未知歌曲" }
                        val artist = cursor.getString(artistColumn).orEmpty().ifBlank { "未知艺术家" }
                        val relativePath = cursor.getString(relativePathColumn).orEmpty()
                        add(
                            MusicTrack(
                                id = "media:$mediaId",
                                title = title,
                                artist = artist,
                                uri = contentUri,
                                durationMillis = cursor.getLong(durationColumn).coerceAtLeast(0L),
                                sourceLabel = if (relativePath.contains("music", ignoreCase = true)) {
                                    "Music"
                                } else {
                                    "媒体库"
                                },
                            ),
                        )
                    }
                }
            }.orEmpty()
        }.onFailure {
            Log.w(TAG, "读取系统媒体库失败", it)
        }.getOrDefault(emptyList())
    }

    private fun loadStorageMusicTracks(): List<MusicTrack> {
        val roots = storageRoots()
        return roots
            .flatMap { root ->
                listOf(File(root, "Music"), File(root, "music"))
                    .filter { it.isDirectory && it.canRead() }
                    .flatMap(::scanMusicDirectory)
            }
            .distinctBy { it.uri.toString() }
    }

    private fun storageRoots(): List<File> {
        val roots = linkedSetOf<File>()
        roots += File("/storage/emulated/0")
        File("/storage").listFiles()
            ?.filter { file ->
                file.isDirectory &&
                    file.canRead() &&
                    file.name !in setOf("self", "emulated")
            }
            ?.forEach { roots += it }
        appContext.getExternalFilesDirs(null)
            .mapNotNull { it?.path?.substringBefore("/Android") }
            .map(::File)
            .filter { it.isDirectory && it.canRead() }
            .forEach { roots += it }
        return roots.toList()
    }

    private fun scanMusicDirectory(directory: File): List<MusicTrack> {
        return runCatching {
            directory.walkTopDown()
                .onEnter { it.canRead() }
                .filter { it.isFile && it.extension.lowercase(Locale.US) in AudioExtensions }
                .map { file ->
                    MusicTrack(
                        id = file.absolutePath,
                        title = file.nameWithoutExtension.ifBlank { file.name },
                        artist = "本地文件",
                        uri = android.net.Uri.fromFile(file),
                        sourceLabel = directory.absolutePath,
                    )
                }
                .toList()
        }.onFailure {
            Log.w(TAG, "扫描音乐目录失败 path=${directory.absolutePath}", it)
        }.getOrDefault(emptyList())
    }

    private companion object {
        const val TAG = "NetShieldMusic"
        val AudioExtensions = setOf("mp3", "wav", "m4a", "aac", "flac", "ogg")
    }
}
