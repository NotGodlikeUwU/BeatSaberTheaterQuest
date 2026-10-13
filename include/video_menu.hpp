#pragma once
#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/RawImage.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "HMUI/InputFieldView.hpp"
DECLARE_CLASS_CODEGEN(Theater, VideoMenuView, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, detailsRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, searchRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, offsetRoot);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, titleLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, metadataLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, statusLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, offsetLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, previewLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, searchLabel);
    DECLARE_INSTANCE_FIELD(HMUI::InputFieldView*, queryInput);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, downloadButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, deleteButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, deleteConfigButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, previewButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, resultDownloadButton);
    DECLARE_INSTANCE_FIELD(ArrayW<UnityEngine::UI::Button*>, resultButtons);
    DECLARE_INSTANCE_FIELD(ArrayW<UnityEngine::UI::RawImage*>, thumbnails);
    DECLARE_INSTANCE_FIELD(StringW, levelID);
    DECLARE_INSTANCE_FIELD(int, page);
    DECLARE_INSTANCE_FIELD(int, selectedIndex);
    DECLARE_INSTANCE_FIELD(uint64_t, shownRevision);
    DECLARE_INSTANCE_FIELD(float, nextRefresh);
    DECLARE_INSTANCE_FIELD(bool, searchOpen);
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
};
void CreateVideoMenu(UnityEngine::Transform* parent);
