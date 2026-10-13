# Quest checks for 0.4.0

Target Beat Saber 1.40.8_7379. Keep the previous QMOD before installing this experimental build.

1. Start the game and enter Solo. Confirm no videos download automatically. Select a configured map: Download Video must be disabled when its MP4 exists. Delete the video and confirm Download unlocks; download it and confirm it locks again.
2. Open Mods → Theater → Search. Search for the selected song, check thumbnails and both result pages, select a result, and download it. Leave and return to the song; verify the chosen title and URL persist. Try a YouTube URL in the search field as well. Use the browser session export if the server requests login.
3. Preview a newly downloaded clip. Verify clip audio is in the left ear and full song audio in the right ear. Use +/-20, +/-100 and +/-1000 ms controls, stop/restart preview, and reopen the song. Verify the offset persists. Close the panel, select another song, or start gameplay: preview must stop and normal audio must return.
4. Repeat selection/search/download/preview/gameplay for an OST and an owned, installed DLC song. Verify files appear under the mod's TheaterOSTVideos directory and no game/DLC files are modified.
5. Use a custom map with Cinema/Theater declared on only one difficulty: verify the video plays only on that difficulty. Check an ordinary map without declarations and a WIP difficulty without a declaration.
6. In a PlaylistCore playlist, provide a matching song's `customData.cinema` or `customData.theater` configuration. Verify it loads from that playlist, a local configuration overrides it, and selecting an unrelated playlist does not reuse the old binding.
7. Test each colorCorrection channel and combined adjustments, and both vignette types. Check logs for `Video filter ready` and absence of filter errors. Turn the head and check both eyes for stable geometry, correct texture orientation, no white/black frame, and readable video. Try at least 1080p30 and 1080p60.
8. Toggle Color blending and Transparent screen body, then restart the song. Test explicit per-video values overriding global settings. Check `Video material` logs; a soft-additive fallback warning means this feature still needs a compatible shader on the target game. Check environment geometry, mirrors, halo and floor lighting.
9. Configure a valid compatible environmentName and verify the requested scene loads. Try an unknown name and an incompatible environment type: gameplay must retain its normal environment and log the rejected override.
10. Delete config and verify local media/config are removed and bundled/playlist bindings remain disabled. Select a new search result and verify it re-enables the binding. Repeat menu/gameplay transitions, pause/resume and quit/relaunch while checking for errors, leaked preview audio or crashes.

Record the installed library hashes, fresh `bstheater` log, map/difficulty/playlist identifiers and the result of each check. Build success alone does not close this checklist.
