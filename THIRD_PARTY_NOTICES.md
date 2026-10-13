# Third-party notices — BeatSaberTheaterQuest

The native port and bundled map bindings derive from neboman11/BeatSaberTheater,
licensed under GNU GPL version 3. See LICENSE and the original source directory.

The downloader embeds these unmodified libraries, translated to Android DEX:

| Component | Version / source | License text included |
|---|---|---|
| NewPipeExtractor | eb53b79e6242d52f0ee2c2614e04e5a9dc2b6a64, https://github.com/TeamNewPipe/NewPipeExtractor/tree/eb53b79e6242d52f0ee2c2614e04e5a9dc2b6a64 | GPL-3.0, LICENSE |
| nanojson | e9d656ddb49a412a5a0a5d5ef20ca7ef09549996, https://github.com/TeamNewPipe/nanojson | Apache-2.0, jsr305-Apache-2.0.txt; Copyright 2011 The nanojson Authors |
| jsoup | 1.22.2, https://jsoup.org | jsoup-MIT.txt |
| JSR-305 annotations | 3.0.2, https://github.com/findbugsproject/findbugs | jsr305-Apache-2.0.txt |
| protobuf-javalite | 4.35.1, https://github.com/protocolbuffers/protobuf/tree/v35.1 | protobuf-BSD.txt |
| Rhino and Rhino engine | 1.8.1, https://github.com/mozilla/rhino/tree/Rhino1_8_1_Release | Rhino-MPL-2.0.txt (also contains notices for bundled third-party code) |
| desugar_jdk_libs_nio | 2.1.5, https://github.com/google/desugar_jdk_libs | desugar-LICENSE.txt, desugar-ASSEMBLY_EXCEPTION.txt |
| QuickJS | 535a7c250ff4a577ec36c3e103daab6dadeea650 (2026-06-04), https://github.com/bellard/quickjs | quickjs-MIT.txt |
| yt-dlp EJS | 0.8.0, https://github.com/yt-dlp/ejs/releases/tag/0.8.0 | ejs-Unlicense.txt |
| meriyah / astring (bundled by EJS) | Versions and original notices in resources/ejs/yt.solver.lib.js | ejs-dependencies.txt (ISC / MIT) |

License texts are in third_party in the source tree and included at the root of the QMOD.
The earlier v0.26.5 NewPipe source is in ../reference/NewPipeExtractor; the current
revision is linked above and pinned in java/dependencies.json. QuickJS source is
included in third_party/quickjs (VERSION renamed VERSION.txt to avoid shadowing
the C++ <version> header on Windows); EJS release assets are in resources/ejs.
Their checksums are recorded in third_party/player-solver.sha256.json.
Dependency download URLs and SHA-256 digests are in java/dependencies*.json.
C++/Java downloader sources, DEX generation scripts, and original map bindings are provided
alongside this build. No PC executable or private hosted download service is required.

Native mod dependencies are separately installed using QMOD manifest dependencies.


SongCore DeleteIcon.png: MIT, copyright (c) 2022 Raine. License included as third_party/songcore-icon-MIT.txt. Source: https://github.com/raineaeternal/Quest-SongCore.

