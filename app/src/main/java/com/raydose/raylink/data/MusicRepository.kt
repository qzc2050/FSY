package com.raydose.raylink.data

import android.content.ContentUris
import android.content.Context
import android.media.MediaMetadataRetriever
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import com.raydose.raylink.model.MusicTrack
import java.io.File
import java.util.Locale

class MusicRepository(context: Context) {
    private val appContext = context.applicationContext

    fun loadTracks(): List<MusicTrack> {
        // 最终按「歌名」合并：同一首歌常被 MediaStore + Music 目录各扫一次
        val byTitle = linkedMapOf<String, MusicTrack>()
        (loadMediaStoreTracks() + loadStorageMusicTracks()).forEach { track ->
            val key = dedupeTitleKey(track)
            byTitle[key] = preferTrack(byTitle[key], track)
        }
        return byTitle.values.sortedWith(
            compareBy<MusicTrack> { it.sourceLabel }
                .thenBy { it.title.lowercase(Locale.getDefault()) },
        )
    }

    private fun dedupeTitleKey(track: MusicTrack): String {
        val name = track.filePath?.let { File(it).nameWithoutExtension } ?: track.title
        return name.trim().lowercase(Locale.getDefault())
    }

    /** 优先保留 MediaStore（有时长/艺术家）；目录扫描重复项丢弃 */
    private fun preferTrack(a: MusicTrack?, b: MusicTrack): MusicTrack {
        if (a == null) return b.ensureDuration()
        val aMedia = a.id.startsWith("media:")
        val bMedia = b.id.startsWith("media:")
        val duration = maxOf(a.durationMillis, b.durationMillis)
        val path = a.filePath ?: b.filePath
        return when {
            bMedia && !aMedia -> b.ensureDuration().copy(
                durationMillis = maxOf(b.durationMillis, duration),
                filePath = b.filePath ?: path,
            )
            aMedia && !bMedia -> a.copy(
                durationMillis = duration,
                filePath = a.filePath ?: path,
            )
            duration > a.durationMillis -> a.copy(durationMillis = duration, filePath = path)
            else -> a.copy(filePath = path)
        }
    }

    private fun loadMediaStoreTracks(): List<MusicTrack> {
        val uri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI
        // 部分工控 ROM（如本机 RK3568）无 DISPLAY_NAME 列，勿强依赖
        val projection = arrayOf(
            MediaStore.Audio.Media._ID,
            MediaStore.Audio.Media.TITLE,
            MediaStore.Audio.Media.ARTIST,
            MediaStore.Audio.Media.DURATION,
            MediaStore.Audio.Media.RELATIVE_PATH,
            MediaStore.Audio.Media.DATA,
        )
        val selection = "${MediaStore.Audio.Media.IS_MUSIC} != 0"
        val sortOrder = "${MediaStore.Audio.Media.TITLE} COLLATE NOCASE ASC"
        return runCatching {
            appContext.contentResolver.query(uri, projection, selection, null, sortOrder)?.use { cursor ->
                val idColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID)
                val titleColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE)
                val artistColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST)
                val durationColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DURATION)
                val relativePathColumn = cursor.getColumnIndex(MediaStore.Audio.Media.RELATIVE_PATH)
                val dataColumn = cursor.getColumnIndex(MediaStore.Audio.Media.DATA)
                buildList {
                    while (cursor.moveToNext()) {
                        val mediaId = cursor.getLong(idColumn)
                        val contentUri = ContentUris.withAppendedId(uri, mediaId)
                        val title = cursor.getString(titleColumn).orEmpty().ifBlank { "未知歌曲" }
                        val artist = cursor.getString(artistColumn).orEmpty().ifBlank { "未知艺术家" }
                        val relativePath = if (relativePathColumn >= 0) {
                            cursor.getString(relativePathColumn).orEmpty()
                        } else {
                            ""
                        }
                        val dataPath = if (dataColumn >= 0) cursor.getString(dataColumn) else null
                        val filePath = resolveMediaFilePath(dataPath, relativePath, title)
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
                                filePath = filePath,
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
                    .distinctBy { directory ->
                        runCatching { directory.canonicalPath }.getOrDefault(directory.absolutePath)
                    }
                    .flatMap(::scanMusicDirectory)
            }
            .distinctBy { track -> track.filePath ?: track.uri.toString() }
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
                        sourceLabel = "Music",
                        filePath = normalizeAbsolutePath(file.absolutePath),
                    )
                }
                .toList()
        }.onFailure {
            Log.w(TAG, "扫描音乐目录失败 path=${directory.absolutePath}", it)
        }.getOrDefault(emptyList())
    }

    fun musicImportDirectory(): File {
        val candidate = File(Environment.getExternalStorageDirectory(), "Music")
        if (!candidate.exists()) {
            candidate.mkdirs()
        }
        return candidate
    }

    /** MediaStore DATA 可能为空：用 relative_path + 文件名拼绝对路径，便于与目录扫描去重。 */
    private fun resolveMediaFilePath(
        dataPath: String?,
        relativePath: String?,
        titleOrName: String?,
    ): String? {
        normalizeAbsolutePath(dataPath)?.let { return it }
        if (relativePath.isNullOrBlank() || titleOrName.isNullOrBlank()) return null
        val base = Environment.getExternalStorageDirectory()
        val dir = File(base, relativePath.trimEnd('/') + "/")
        // title 通常无扩展名；在 Music 目录下按「同名.*」匹配真实文件
        val exact = File(dir, titleOrName)
        if (exact.isFile) return normalizeAbsolutePath(exact.absolutePath)
        val matched = dir.listFiles()
            ?.firstOrNull { file ->
                file.isFile &&
                    file.extension.lowercase(Locale.US) in AudioExtensions &&
                    file.nameWithoutExtension.equals(titleOrName, ignoreCase = true)
            }
        return matched?.let { normalizeAbsolutePath(it.absolutePath) }
    }

    private fun MusicTrack.ensureDuration(): MusicTrack {
        if (durationMillis > 0L) return this
        val path = filePath ?: return this
        val ms = readDurationMillis(path) ?: return this
        return copy(durationMillis = ms)
    }

    private fun readDurationMillis(path: String): Long? {
        val retriever = MediaMetadataRetriever()
        return runCatching {
            retriever.setDataSource(path)
            retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)
                ?.toLongOrNull()
                ?.takeIf { it > 0L }
        }.onFailure {
            Log.w(TAG, "读取时长失败 path=$path", it)
        }.getOrNull().also {
            runCatching { retriever.release() }
        }
    }

    private fun normalizeAbsolutePath(path: String?): String? {
        if (path.isNullOrBlank()) return null
        return runCatching { File(path).canonicalPath }.getOrDefault(File(path).absolutePath)
    }

    private companion object {
        const val TAG = "RaylinkMusic"
        val AudioExtensions = setOf("mp3", "wav", "m4a", "aac", "flac", "ogg")
    }
}
