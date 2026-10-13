# BeatSaberTheater Quest 0.1

A native Theater port from PC Beat Saber for Beat Saber Quest **1.40.8_7379**, built with Scotland2. Based on neboman11/BeatSaberTheater. Made with ChatGPT 6.1

## Use

Install `BeatSaberTheaterQuest-0.1.qmod` with a BMBF/QuestPatcher.

First, you need to log in to YouTube through the Quest Browser. Then launch the game, select a custom map with Cinema support. A **Video available / DOWNLOAD VIDEO** row appears between the difficulty buttons and Play/Practice. Start a download with this button. Also you can download your own videos for any Beatmap using video search. Also you can change play offsets.

## Video quality and YouTube

The downloader selects the best available H.264 stream up to **1080p**, preferring **60 fps** when resolutions are equal. HLS is supported for authenticated browser clients: segments are downloaded to the Quest, and Android writes the video track to MP4 without re-encoding.

The default limits are 1080p, 900 seconds, and 512 MiB. Change them in **Settings → Mod Settings → Theater**. Temporary HLS segments and the completed MP4 require sufficient free storage.

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
```

To copy a development build to an installed game, run `scripts/copy.ps1 -Serial <serial>`; the script keeps backups of both libraries. Check a generated package with `scripts/verify-package.py`. The validation notes and known limitations are in `VALIDATION.md`.

## Sources

- [Quest configuration guide](https://bsmg.wiki/modding/quest/config.html)
- [NewPipeExtractor pinned source](https://github.com/TeamNewPipe/NewPipeExtractor/tree/eb53b79e6242d52f0ee2c2614e04e5a9dc2b6a64)
- [yt-dlp EJS 0.8.0](https://github.com/yt-dlp/ejs/releases/tag/0.8.0)
- [yt-dlp YouTube video formats](https://github.com/yt-dlp/yt-dlp/blob/master/yt_dlp/extractor/youtube/_video.py)
- [Meta Quest Browser remote debugging](https://developers.meta.com/vr/documentation/web/browser-remote-debugging/)
