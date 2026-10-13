#include "bsml/shared/BSML.hpp"
#include "modconfig.hpp"
#include "downloads.hpp"
#include "download_status_view.hpp"
#include "video_download_row.hpp"
#include "video_menu.hpp"
#include "HMUI/ViewController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include "TMPro/TextOverflowModes.hpp"
#include "custom-types/shared/register.hpp"

namespace {
void ApplyDownloadOptions() {
    theater::SetDownloadOptions(getModConfig().AutoDownload.GetValue(),getModConfig().MaxVideoHeight.GetValue(),getModConfig().DownloadTimeout.GetValue(),getModConfig().MaxVideoMiB.GetValue());
}

}
void RegisterTheaterSettings() {
    custom_types::Register::AutoRegister();
    BSML::Init();
    InstallVideoDownloadRow();
    BSML::Register::RegisterSettingsMenu("Theater", [](HMUI::ViewController* self, bool first, bool, bool) {
        if (!first) return;
        auto parent = BSML::Lite::CreateScrollableSettingsContainer(self->get_transform())->get_transform();
        BSML::Lite::CreateText(parent,"BeatSaberTheater Quest");
        BSML::Lite::CreateToggle(parent,"Enable videos",getModConfig().Enabled.GetValue(),[](bool value) { getModConfig().Enabled.SetValue(value); });
        BSML::Lite::CreateToggle(parent,"Color blending",getModConfig().ColorBlendingEnabled.GetValue(),[](bool value) { getModConfig().ColorBlendingEnabled.SetValue(value); });
        BSML::Lite::CreateToggle(parent,"Transparent screen body",getModConfig().TransparencyEnabled.GetValue(),[](bool value) { getModConfig().TransparencyEnabled.SetValue(value); });
        BSML::Lite::CreateIncrementSetting(parent,"Maximum video height",0,180.f,getModConfig().MaxVideoHeight.GetValue(),360.f,1080.f,[](float value) { getModConfig().MaxVideoHeight.SetValue(static_cast<int>(value)); ApplyDownloadOptions(); });
        BSML::Lite::CreateIncrementSetting(parent,"Download timeout (seconds)",0,30.f,getModConfig().DownloadTimeout.GetValue(),30.f,1800.f,[](float value) { getModConfig().DownloadTimeout.SetValue(static_cast<int>(value)); ApplyDownloadOptions(); });
        BSML::Lite::CreateIncrementSetting(parent,"Maximum video size (MiB)",0,128.f,getModConfig().MaxVideoMiB.GetValue(),128.f,2048.f,[](float value) { getModConfig().MaxVideoMiB.SetValue(static_cast<int>(value)); ApplyDownloadOptions(); });
        BSML::Lite::CreateIncrementSetting(parent,"Sync tolerance (seconds)",2,.05f,getModConfig().SyncTolerance.GetValue(),.1f,2.f,[](float value) { getModConfig().SyncTolerance.SetValue(value); });
        BSML::Lite::CreateIncrementSetting(parent,"Prepare timeout (seconds)",0,5.f,getModConfig().PrepareTimeout.GetValue(),5.f,120.f,[](float value) { getModConfig().PrepareTimeout.SetValue(value); });
    },false);
    BSML::Register::RegisterGameplaySetupTab("Theater",[](UnityEngine::GameObject* self,bool first) {
        if (!first) return;
        CreateVideoMenu(self->get_transform());
    },BSML::MenuType::Solo);
}
