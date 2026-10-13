# Validation — 0.3.7 — 2026-10-13

Target: Beat Saber Quest 1.40.8_7379 / Scotland2 / ARM64.

Baseline: rolled-back 0.3.5. The existing 0.3.5 QMOD is preserved.

Changes in 0.3.7:
- Download Video in the song detail row is disabled when the selected map has a non-empty local video file or a download is queued/in progress.
- Download Video / Retry in Theater settings and its gameplay tab use the same availability rule.
- RetrySelectedDownload rejects an already available video or pending download before queueing it.
- Deleting a video through the existing trash button re-enables download after the next availability refresh (0.25 seconds in the detail row; 0.5 seconds in Theater).
- Rendering, glow, and Java downloader code are unchanged.

Validation:
- Native CMake/NDK build passed.
- QMOD verification passed: embedded DEX/EJS, source, and dependency hashes were checked.
- No Quest was connected in `adb devices`, so installation, the disabled-button appearance, and the download/delete interaction were not verified live in this revision.
- Previous validation and rollback evidence is documented in the original 0.3.5 validation record, which is kept locally.

SHA-256:
- `libbstheater.so`: `7F4C1AE25240DD36C73C2A9D905C4C25B002B021D67DCF5484132DC6E233155D`
- `libbstheaterjs.so`: `2023556C742474431554A08E0C2961B07205D2F41A478A74561F1E9FE19FE228`
- QMOD: `6739468DD41BBFAD18FD1D6A0E05511D393808E7953396A4CE123D63C039FE23`
