#pragma once
#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
DECLARE_CLASS_CODEGEN(Theater, VideoDownloadRow, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, label);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, button);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, deleteButton);
    DECLARE_INSTANCE_FIELD(float, nextRefresh);
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, DeleteVideoClick);
};
void InstallVideoDownloadRow();
