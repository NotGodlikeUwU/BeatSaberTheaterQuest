# BeatSaberTheater Quest 0.4.0

A native ARM64 port for Beat Saber Quest **1.40.8_7379**, built with Scotland2. Based on neboman11/BeatSaberTheater, licensed under GPL-3.0. The original PC source is maintained separately.

## Use

Install `BeatSaberTheaterQuest-0.4.0.qmod` with a Quest mod manager, then restart the game. This is an experimental feature build: the new menus, audio preview, GPU filters, and environment selection have not yet been tested in a headset. Keep your previous QMOD for rollback.

Select a custom map with `theater-video.json`, `cinema-video.json`, or a Cinema binding from the original PC mod. A **Video available / DOWNLOAD VIDEO** row appears between the difficulty buttons and Play/Practice. Start a download manually with this button. Videos are not downloaded automatically when the game starts, the song list loads, a map is selected, or a song begins.

The small trash button beside the download control removes the MP4 and its quality information for the selected map. The map and its configuration are kept. The trash button is disabled while a download is running. After deleting the video, you can download it again.

The mod registers Cinema, Theater, and BeatSaberTheater capabilities with SongCore. It supports the original PC mod's database of 136 bindings. Configuration priority is: local `theater-video.json`, local `cinema-video.json`, the selected playlist's binding, then the bundled database. A song without a binding can be configured through the YouTube search menu.

### YouTube menu and synchronization preview

Open **Mods → Theater** on the song selection screen. Search opens a keyboard field prefilled with the song title and artist. Enter a search query or a YouTube URL, choose a result, then press Download. Results include title, uploader, duration, and thumbnail; four results are shown per page. Selecting a result saves a local configuration for that song. An existing downloaded video must be deleted before it can be replaced.

Press **Preview** to play the video on a screen in the menu. The clip's audio plays in the left ear and the full beatmap audio in the right ear, following the PC synchronization workflow. The offset controls change timing by 20, 100, or 1000 milliseconds and update the preview immediately. Changes persist in the local configuration while preserving mapper settings and the original offset. Stop preview, close the Theater panel, or select another song to restore normal menu music. Old cached MP4 files without an audio track need to be deleted and downloaded again for the split-audio preview.

**Delete Video** removes only the media and its quality sidecar. **Delete config** removes the media and local video configurations, and records a local disable marker so a playlist or bundled binding does not immediately return. Choosing a new search result clears that marker.

### OST, DLC, suggestions, and playlists

The same selection, search, download, preview, and playback flow accepts official OST and installed, owned DLC songs. Their configurations and videos are kept in `/sdcard/ModData/com.beatgames.beatsaber/Mods/bstheater/TheaterOSTVideos/`, in a separate directory for each level; the game and DLC assets are not modified.

For custom songs, the selected difficulty follows the PC suggestion rules. If any difficulty declares Cinema/Theater, only difficulties that declare it play the configured video. Songs without those declarations can play videos on all difficulties; WIP difficulties without a declaration are excluded. Both suggestions and requirements are recognized.

PlaylistCore/PlaylistManager playlists can supply a video configuration in a song's `customData.theater` or `customData.cinema`. Bindings match custom song hashes or official `levelid` values. Only the currently selected playlist supplies a binding; local configuration takes priority. PlaylistCore itself is not a required dependency of Theater.

## Video quality and YouTube

The downloader selects the best available H.264 stream up to **1080p**, preferring **60 fps** when resolutions are equal. HLS is supported for authenticated browser clients: segments are downloaded to the Quest, and Android writes MP4 without re-encoding. Available AAC audio is retained or downloaded separately and muxed into the file for the synchronization preview. If optional audio is unavailable, the video can still be saved. Gameplay uses the beatmap audio.

After a video has downloaded, the Download Video buttons in the song card and Theater menu turn gray and become unavailable. They unlock automatically after the video is deleted. To replace a downloaded file, delete it first. The downloader compares the available authenticated clients by resolution and frame rate, so an early 240p HLS result does not prevent it from choosing 1080p from WEB. A `.bstheater-quality` sidecar records the selected quality next to the MP4. Direct HTTPS MP4 streams are also supported.

The default limits are 1080p, 900 seconds, and 512 MiB. Change them in **Settings → Mod Settings → Theater**. Temporary HLS segments and the completed MP4 require sufficient free storage.

The `HLS timestamps are discontinuous` error was fixed by accounting for the difference between H.264 B-frame decode order and presentation order. A video used by map `5405c` (`XKZIQlqVjjk`) was checked at **1920×1080, 60.00 fps, 162916 ms**, and a frame decoded successfully with MediaCodec on Quest 3.

### Meta Quest Browser session

Android isolates the browser from Beat Saber. Sign in to YouTube in Meta Quest Browser, leave the YouTube tab open, connect the Quest over USB with debugging enabled, and run:

```powershell
pwsh scripts/export-quest-youtube-cookies.ps1
```

The script reads YouTube cookies through the official browser DevTools interface, saves them to `private/youtube-cookies.txt`, and imports the file to `/sdcard/ModData/com.beatgames.beatsaber/Mods/bstheater/`. Cookie values are not printed or included in the QMOD. The mod sends the session only over HTTPS to youtube.com; account data is not sent to CDNs or third-party services. Cookie updates received from YouTube are saved locally. If the session expires or is rejected, export the cookies again and press Retry.

## Video screen and environment

The mod supports offset, playback speed, looping, `endVideoAt`, screen position/rotation/size/curvature, and up to seven additional screens. Playback follows the game's AudioSource, pause, and seek state.

Video glow is a transparent halo in the game scene, parented to each video screen and offset downward by 5% of the screen height. Its color comes from a reduced and blurred video frame and updates ten times per second. The area over the clip is transparent, and halo brightness and opacity are capped. It passes through the same VR camera as the screen and does not use separate head matrices. The mod no longer hooks BloomPrePass or changes stereo keywords; the game's regular Bloom remains enabled. This differs from the PC mod's screen-space bloom pass. Environment lighting from the frame's average color through a DirectionalLight is retained. The screen uses the Environment layer for reflections in locations that support them. On Quest, the frame is reduced to one pixel and read at a limited rate for lighting color; this differs from the PC mod's asynchronous readback.

The port includes changes to standard environments, including BTS column movement and HUD tracking in GlassDesert. It supports `bloom`, `disableDefaultModifications`, and `environment[]` settings (`active`, `position`, `rotation`, `scale`, `parentName`, `cloneFrom`). It also checks for opaque geometry in front of a screen: obstructing decorations and rings are hidden during video playback and restored after the song. This check excludes GameCore objects such as cubes, walls, and sabers, as well as the floor and mirrors.

The video configuration also supports these PC fields:

| Field | Quest behavior |
| --- | --- |
| `colorCorrection` | `brightness`, `contrast`, `saturation`, `hue`, `exposure`, and `gamma` use the PC shader's order and equations. Processing stays at the decoded video resolution. |
| `vignette` | Radius, softness, and the PC rectangular/rounded oval shape are applied to the video texture. |
| `transparency` | Preserves the PC configuration convention: `true` requests an opaque screen body. The global Transparent screen body setting supplies the default. |
| `colorBlending` | Selects the PC soft additive blend (`OneMinusDstColor`, `One`) or opaque color replacement (`One`, `Zero`). A per-video value overrides the global Color blending setting. |
| `environmentName` | Selects a known environment before the gameplay scene loads, when a downloaded video is present and the environment matches the characteristic's environment type. Unknown or incompatible names retain the game's selected environment. |

Color correction and vignette currently require the game's OpenGLES3 backend. A failed GPU filter logs the error and displays the original video. Soft additive blending uses a compatible stock unlit particle shader; if the required shader or blend properties were stripped from the game, the mod logs a warning and uses the ordinary screen material. These fallback paths preserve playback but do not reproduce the requested effect. Their behavior and VR appearance still require Quest validation. The existing world-space halo and environment light are retained; no new bloom-camera or head-tracking path was added.

## Build

The build requires PowerShell 7, a JDK, CMake/Ninja, QPM, Android NDK r27d, Android API 33 `android.jar`, R8, and `desugar_jdk_libs_nio`. The build scripts expect the local QPM, Android, and R8 tools under a sibling `../tools` directory. Native dependencies are declared in the QPM manifests. Java dependencies can be downloaded and SHA-256 verified with `scripts/restore-java.ps1`.

From the repository root, run:

```powershell
pwsh scripts/restore-java.ps1
pwsh scripts/build.ps1
../tools/qpm/qpm.exe qmod zip --skip_build --offline
pwsh scripts/test.ps1
pwsh scripts/test-java.ps1
pwsh scripts/test-cookie-export.ps1
python scripts/verify-package.py
```

The optional host GPU check runs the shipped GLSL on a real offscreen context: install `moderngl`, then run `python scripts/test-video-filter.py`. This does not validate Unity's Quest render-thread integration.

To copy a development build to an installed game, run `scripts/copy.ps1 -Serial <serial>`; the script keeps backups of both libraries. Check a generated package with `scripts/verify-package.py`. The validation notes and known limitations are in `VALIDATION.md`.

## Sources

- [Quest configuration guide](https://bsmg.wiki/modding/quest/config.html)
- [NewPipeExtractor pinned source](https://github.com/TeamNewPipe/NewPipeExtractor/tree/eb53b79e6242d52f0ee2c2614e04e5a9dc2b6a64)
- [yt-dlp EJS 0.8.0](https://github.com/yt-dlp/ejs/releases/tag/0.8.0)
- [yt-dlp YouTube video formats](https://github.com/yt-dlp/yt-dlp/blob/master/yt_dlp/extractor/youtube/_video.py)
- [Meta Quest Browser remote debugging](https://developers.meta.com/vr/documentation/web/browser-remote-debugging/)
- [Quest PlaylistCore](https://github.com/Metalit/PlaylistCore)
- [Unity render-thread plugin events](https://docs.unity3d.com/2021.3/Documentation/ScriptReference/GL.IssuePluginEvent.html)
