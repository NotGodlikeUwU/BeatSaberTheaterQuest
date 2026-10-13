#pragma once
#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "UnityEngine/UI/Button.hpp"
DECLARE_CLASS_CODEGEN(Theater, DownloadStatusView, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, label);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, downloadButton);
    DECLARE_INSTANCE_FIELD(float, nextRefresh);
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_METHOD(void, Update);
};

