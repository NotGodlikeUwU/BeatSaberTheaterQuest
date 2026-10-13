# Validation — 0.4.1 — 2026-10-13

Target: Beat Saber Quest 1.40.8_7379 / Scotland2 / ARM64.

## Theater menu crash fix

The installed 0.4.0 library matched SHA-256 `8D3A52C8528C908724ABF9DD8F45F8DC65DF97A4CB83F601164030D6B6DADD4E`. Quest logcat recorded an uncaught `RawImage::set_texture` NullException while opening Theater on 2026-10-13 at 23:02:21 Moscow time. The native backtrace entered `BSML::Lite::CreateRawImage`, then `CreateVideoMenu` at source line 151 and the Theater gameplay-tab callback. Symbolication used the matching debug library.

BSML 0.4.55's Lite RawImage factory uses ImageTag, which creates HMUI::ImageView, then asks it for a RawImage component and dereferences the missing component. Theater 0.4.1 creates its thumbnail GameObject and RawImage directly, using the same UI material. Its MonoBehaviour constructor now follows the existing custom-types initialization pattern. Updates begin only after construction finishes; thumbnail cleanup checks Unity destroyed-object semantics.

The native build and package checks pass. A temporary device probe created the full VideoMenuView on Quest 3 inside Beat Saber 1.40.8_7379, verified four live RawImage components, and ran three activation/deactivation cycles with details/search updates. Logcat recorded `MENU_SMOKE_PASS` at 23:13:15 Moscow time; the process remained alive. The probe was removed before building the delivered package. This verifies construction and programmatic lifecycle, not visual layout, controller input, or a user opening the Settings and gameplay tabs. The original user interaction still requires confirmation when the user can operate the headset. Final artifact hashes are recorded below.

### Final 0.4.1 artifacts and installation

- `libbstheater.so`: `8BEB413EC24CCABDAAAF87CD0B010FB15E0C72373481532CA967A44091436FC3`
- `libbstheaterjs.so`: `2023556C742474431554A08E0C2961B07205D2F41A478A74561F1E9FE19FE228`
- `BeatSaberTheaterQuest-0.4.1.qmod`: `66D2D551E4BFCF4D394EADC5A2C3E18EF3A137A79C8519660E831D21471BEC0F`

Both final libraries were copied to Quest 3 after removing the temporary probe; installed hashes matched and the game was restarted. Package verification rejects binaries containing the temporary probe marker. The original installed 0.4.0 libraries are preserved in `backups/20261013-230734927/`.

## Previous 0.4.0 feature validation

This is an experimental feature build. The user requested a prepared package because the Quest is unavailable. `adb devices -l` returned no devices; no installation or headset behavior is claimed.

## Baseline and scope

The source branch starts from published 0.3.7, retaining manual-only downloads and disabling Download Video while the file exists. Its QMOD and source are preserved locally in `backups/pc-parity-20261013-211942/`. The earlier 0.3.5 rendering control is also retained. No bloom camera, BloomPrePass hook, stereo keyword change, or head matrix path was added. `src/screen_effects.cpp` is unchanged.

Changes implement YouTube search and video selection, a synchronization preview with split audio, persistent visual offset controls, official OST/owned installed DLC storage, selected-difficulty suggestion rules, selected playlist bindings, color correction/vignette processing, screen transparency/color blending, and environment selection before scene loading. New downloads retain or mux AAC when available for preview; gameplay still uses beatmap audio.

## Completed host checks

- Native CMake/NDK r27d build: ARM64, API 32.
- 199 native configuration checks: bundled bindings, local/playlist priority, official level bindings, offset persistence, effect parameters, deletion markers, invalid input.
- 24 Java download checks; 48 YouTube session checks; 9 HLS manifest checks, including AAC group selection; 6 offline search validation checks.
- A real public search for `TAK PPPP`, without user cookies, returned 14 results and passed 63 checks. This caught and fixed the mobile User-Agent redirect that broke NewPipe's WEB client bootstrap. It is host network evidence, not Quest session evidence.
- Cookie export integration passed with a synthetic session. No real cookie values were printed or included in the package/source.
- 15 checks ran the shipped GLSL on an offscreen NVIDIA GeForce RTX 3070 context: identity, all six correction controls, PC Rodrigues hue/additive brightness/order, PC vignette shapes, and soft additive blending.
- The shader equations were compared against the supplied PC bundle's disassembled pixel program. GPU tests do not validate Unity's Quest render-thread integration or stock shader availability.

The QMOD check compares packaged libraries with build output, verifies AArch64 ELF headers, embedded DEX/EJS and tested GLSL, pinned dependency hashes, manifest versions/dependencies, license files, and absence of cookies. Artifact hashes are recorded below after packaging.

## Still requiring Quest

Use [HEADSET_CHECKLIST.md](HEADSET_CHECKLIST.md). The new BSML layout and buttons, full-song asynchronous audio loading/unloading, Android H.264/AAC mux and preview, render-thread texture ordering, stereo appearance, stock particle shader blending, reflection/body behavior, suggestion gating, and environment transition need live testing.

Color correction/vignette require OpenGLES3. Unsupported backends or failed GPU jobs keep the original video and log a warning/error. If a compatible unlit shader with dynamic blend properties is unavailable, playback uses the ordinary material and logs a warning; color blending is then not reproduced. No universal PC/Quest visual equivalence is claimed.

Older cached silent videos require manual deletion/redownload for split-audio preview. Search and downloads may still be rejected by YouTube for an expired session or restricted video. Installed/owned DLC is required for its map audio. No startup-crash fix or headset performance result is claimed in this build.

## Previous 0.4.0 packaged artifact hashes

QMOD verification passed on the final build:

- `libbstheater.so`: `8D3A52C8528C908724ABF9DD8F45F8DC65DF97A4CB83F601164030D6B6DADD4E`
- `libbstheaterjs.so`: `2023556C742474431554A08E0C2961B07205D2F41A478A74561F1E9FE19FE228`
- `BeatSaberTheaterQuest-0.4.0.qmod`: `E0F9AD4F5E7CFAB507B2F46F825DA28D9BF0EEBBE8D2CC3F41FC45FAAAC02D71`
