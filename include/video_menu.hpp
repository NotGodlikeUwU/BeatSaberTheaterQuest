#pragma once
#include "custom-types/shared/macros.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "HMUI/CurvedTextMeshPro.hpp"
#include "HMUI/ImageView.hpp"
#include "HMUI/TableView.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "bsml/shared/BSML/Components/Keyboard/ModalKeyboard.hpp"
#include "bsml/shared/BSML/Components/Settings/ToggleSetting.hpp"
DECLARE_CLASS_CODEGEN(Theater, VideoMenuView, UnityEngine::MonoBehaviour) {
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, menuRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, noVideoRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, detailsRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, searchRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, offsetRoot);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, customizeOffsetRoot);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, noVideoLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, titleLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, authorLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, durationLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, statusLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, offsetLabel);
    DECLARE_INSTANCE_FIELD(HMUI::CurvedTextMeshPro*, searchLabel);
    DECLARE_INSTANCE_FIELD(HMUI::ImageView*, detailsThumbnail);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, mediaButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, deleteConfigButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, previewButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, searchButton);
    DECLARE_INSTANCE_FIELD(UnityEngine::UI::Button*, resultDownloadButton);
    DECLARE_INSTANCE_FIELD(BSML::CustomListTableData*, resultsList);
    DECLARE_INSTANCE_FIELD(BSML::ModalKeyboard*, queryKeyboard);
    DECLARE_INSTANCE_FIELD(BSML::ToggleSetting*, customizeToggle);
    DECLARE_INSTANCE_FIELD(StringW, levelID);
    DECLARE_INSTANCE_FIELD(StringW, query);
    DECLARE_INSTANCE_FIELD(int, selectedIndex);
    DECLARE_INSTANCE_FIELD(uint64_t, shownRevision);
    DECLARE_INSTANCE_FIELD(float, nextRefresh);
    DECLARE_INSTANCE_FIELD(bool, searchOpen);
    DECLARE_INSTANCE_FIELD(bool, initialized);
    DECLARE_INSTANCE_FIELD(bool, customizeOffset);
    DECLARE_CTOR(ctor);
    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);
    DECLARE_INSTANCE_METHOD(void, SearchClick);
    DECLARE_INSTANCE_METHOD(void, RefineClick);
    DECLARE_INSTANCE_METHOD(void, QueryEntered, StringW text);
    DECLARE_INSTANCE_METHOD(void, BackClick);
    DECLARE_INSTANCE_METHOD(void, MediaClick);
    DECLARE_INSTANCE_METHOD(void, DeleteConfigClick);
    DECLARE_INSTANCE_METHOD(void, PreviewClick);
    DECLARE_INSTANCE_METHOD(void, DownloadClick);
    DECLARE_INSTANCE_METHOD(void, SelectCell, HMUI::TableView* table, int index);
    DECLARE_INSTANCE_METHOD(void, OffsetMinusHigh);
    DECLARE_INSTANCE_METHOD(void, OffsetMinusMid);
    DECLARE_INSTANCE_METHOD(void, OffsetMinusLow);
    DECLARE_INSTANCE_METHOD(void, OffsetPlusHigh);
    DECLARE_INSTANCE_METHOD(void, OffsetPlusMid);
    DECLARE_INSTANCE_METHOD(void, OffsetPlusLow);
};
void CreateVideoMenu(UnityEngine::Transform* parent);
