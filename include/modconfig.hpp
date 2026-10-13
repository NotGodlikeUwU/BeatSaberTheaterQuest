#pragma once
#include "config-utils/shared/config-utils.hpp"
DECLARE_CONFIG(ModConfig) {
    CONFIG_VALUE(Enabled, bool, "Enabled", true);
    CONFIG_VALUE(SyncTolerance, float, "Sync tolerance (seconds)", 0.3f);
    CONFIG_VALUE(PrepareTimeout, float, "Prepare timeout (seconds)", 20.0f);
    CONFIG_VALUE(AutoDownload, bool, "Automatically download configured map videos", false);
    CONFIG_VALUE(MaxVideoHeight, int, "Maximum video height", 1080);
    CONFIG_VALUE(HDDownloadsVersion, int, "HD download defaults version", 0);
    CONFIG_VALUE(DownloadTimeout, int, "Download timeout (seconds)", 900);
    CONFIG_VALUE(MaxVideoMiB, int, "Maximum video size (MiB)", 512);
    CONFIG_VALUE(ColorBlendingEnabled, bool, "Color blending", true);
    CONFIG_VALUE(TransparencyEnabled, bool, "Transparent screen body", true);
};
