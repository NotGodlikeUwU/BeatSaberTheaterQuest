#include "video_download_row.hpp"
#include "downloads.hpp"
#include "main.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/Components/ButtonIconImage.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include <vector>
#include "trash_icon.inc"
DEFINE_TYPE(Theater,VideoDownloadRow);
void Theater::VideoDownloadRow::ctor(){INVOKE_CTOR();UnityEngine::MonoBehaviour::_ctor();label=nullptr;button=nullptr;deleteButton=nullptr;nextRefresh=0;}
void Theater::VideoDownloadRow::DeleteVideoClick(){theater::DeleteSelectedVideo();}
void Theater::VideoDownloadRow::Update(){
    float now=UnityEngine::Time::get_realtimeSinceStartup();if(now<nextRefresh || !label || !button)return;nextRefresh=now+.25f;
    try {
        auto state=theater::SelectedDownload();
        label->get_gameObject()->SetActive(state.configured);button->get_gameObject()->SetActive(state.configured);
        if(deleteButton){deleteButton->get_gameObject()->SetActive(state.configured);deleteButton->set_interactable(state.ready && !state.busy);}
        if(!state.configured)return;
        label->set_text(state.text);button->set_interactable(!state.ready && !state.busy);
        BSML::Lite::SetButtonText(button,state.busy?"DOWNLOADING":"DOWNLOAD VIDEO");
    }catch(const std::exception& error){PaperLogger.error("Video download row: {}",error.what());}
}
namespace {
void Position(UnityEngine::RectTransform* rect,UnityEngine::Vector2 position,UnityEngine::Vector2 size) {
    rect->set_anchorMin({.5f,.5f});rect->set_anchorMax({.5f,.5f});rect->set_pivot({.5f,.5f});
    rect->set_anchoredPosition(position);rect->set_sizeDelta(size);
    auto fitter=rect->get_gameObject()->GetComponent<UnityEngine::UI::ContentSizeFitter*>();
    if(fitter){fitter->set_horizontalFit(UnityEngine::UI::ContentSizeFitter_FitMode::Unconstrained);fitter->set_verticalFit(UnityEngine::UI::ContentSizeFitter_FitMode::Unconstrained);}
}
}
MAKE_HOOK_MATCH(TheaterDetailAwake,&GlobalNamespace::StandardLevelDetailView::Awake,void,GlobalNamespace::StandardLevelDetailView* self){
    TheaterDetailAwake(self);
    if(!self->_buttonsWrapper)return;
    if(self->_buttonsWrapper->get_transform()->Find("TheaterVideoDownloadRow"))return;
    auto object=UnityEngine::GameObject::New_ctor("TheaterVideoDownloadRow");
    auto rect=object->AddComponent<UnityEngine::RectTransform*>();rect->SetParent(self->_buttonsWrapper->get_transform(),false);
    object->AddComponent<UnityEngine::UI::LayoutElement*>()->set_ignoreLayout(true);
    Position(rect,{0,9},{78,6});
    auto row=object->AddComponent<Theater::VideoDownloadRow*>();
    row->label=BSML::Lite::CreateText(rect,"Video available",3.f,{-25,0},{30,6});
    Position(row->label->get_rectTransform(),{-25,0},{30,6});
    row->label->set_richText(false);row->label->set_alignment(TMPro::TextAlignmentOptions::Center);
    row->button=BSML::Lite::CreateUIButton(rect,"DOWNLOAD VIDEO",{9,0},{34,6},[]{theater::RetrySelectedDownload();});
    Position(row->button->GetComponent<UnityEngine::RectTransform*>(),{9,0},{34,6});
    // Use the same icon-button component and sprite as SongCore's song-delete UI.
    // Its background supplies the full hit target, independently of the small icon.
    BSML::parse_and_construct("<icon-button id='deleteButton' pref-width='9' pref-height='8' on-click='DeleteVideoClick'/>",rect,row);
    if(!row->deleteButton)throw std::runtime_error("Video delete icon-button binding failed");
    Position(row->deleteButton->GetComponent<UnityEngine::RectTransform*>(),{31,0},{9,8});
    auto icon=row->deleteButton->GetComponent<BSML::ButtonIconImage*>();
    icon->SetIcon(BSML::Lite::VectorToSprite(trashIconBytes));
    Position(icon->image->GetComponent<UnityEngine::RectTransform*>(),{0,0},{8,8});
    icon->image->get_transform()->set_localScale({1.7f,1.7f,1.7f});
    icon->image->set_raycastTarget(false);
    row->label->get_gameObject()->SetActive(false);row->button->get_gameObject()->SetActive(false);
    row->deleteButton->get_gameObject()->SetActive(false);
    PaperLogger.info("Cinema download row and video delete icon-button created");
}
void InstallVideoDownloadRow(){INSTALL_HOOK(PaperLogger,TheaterDetailAwake);}
