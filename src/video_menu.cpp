#include "video_menu.hpp"
#include "downloads.hpp"
#include "preview.hpp"
#include "main.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include "UnityEngine/UI/HorizontalLayoutGroup.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "UnityEngine/Networking/UnityWebRequestTexture.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/DownloadHandlerTexture.hpp"
#include "UnityEngine/Time.hpp"
#include "TMPro/TextOverflowModes.hpp"
#include "TMPro/TextAlignmentOptions.hpp"
#include <unordered_map>
#include <limits>
#include <cmath>
DEFINE_TYPE(Theater, VideoMenuView);
namespace {
using namespace UnityEngine;
struct Thumbnails {
    struct Pending {Networking::UnityWebRequest* request;uint32_t handle;int row;};
    std::vector<Pending> pending;
    std::vector<std::pair<Texture*,uint32_t>> textures;
};
std::unordered_map<Theater::VideoMenuView*,Thumbnails> images;
void ClearImages(Theater::VideoMenuView* view) {
    if(view->thumbnails)for(auto image:view->thumbnails)if(image&&Object::op_Implicit_bool(image))image->set_texture(nullptr);
    auto found=images.find(view);if(found==images.end())return;
    for(auto& request:found->second.pending){request.request->Abort();request.request->Dispose();il2cpp_functions::gchandle_free(request.handle);}
    for(auto [texture,handle]:found->second.textures){if(Object::op_Implicit_bool(texture))Object::Destroy(texture);il2cpp_functions::gchandle_free(handle);}
    images.erase(found);
}
void Size(Component* object,float width,float height) {
    auto layout=object->get_gameObject()->GetComponent<UI::LayoutElement*>();
    if(!layout)layout=object->get_gameObject()->AddComponent<UI::LayoutElement*>();
    layout->set_preferredWidth(width);layout->set_minWidth(width);layout->set_preferredHeight(height);
    layout->set_flexibleWidth(0);layout->set_flexibleHeight(0);
}
HMUI::CurvedTextMeshPro* Label(Transform* parent,const std::string& value,float height=7,float font=3) {
    auto label=BSML::Lite::CreateText(parent,value,font);label->set_richText(false);
    label->set_enableWordWrapping(true);label->set_overflowMode(TMPro::TextOverflowModes::Ellipsis);Size(label,78,height);return label;
}
UI::HorizontalLayoutGroup* Row(Transform* parent) {
    auto row=BSML::Lite::CreateHorizontalLayoutGroup(parent);row->set_spacing(1);
    row->set_childForceExpandWidth(false);row->set_childForceExpandHeight(false);Size(row,78,8);return row;
}
UI::RawImage* Thumbnail(Transform* parent) {
    // BSML 0.4.55 Lite::CreateRawImage uses ImageTag (HMUI::ImageView),
    // then dereferences its missing RawImage. Build the correct component.
    auto object=GameObject::New_ctor("TheaterVideoThumbnail");
    auto image=object->AddComponent<UI::RawImage*>();
    image->set_material(BSML::Helpers::GetUINoGlowMat());
    auto rect=image->get_rectTransform();rect->SetParent(parent,false);
    rect->set_anchoredPosition({0,0});rect->set_sizeDelta({18,10});
    image->set_raycastTarget(false);Size(image,18,11);
    return image;
}
void Action(Theater::VideoMenuView* view,const std::function<void()>& action) {
    try{view->previewLabel->set_text("");action();view->nextRefresh=0;}
    catch(const std::exception& error){(view->searchOpen?view->searchLabel:view->previewLabel)->set_text(error.what());PaperLogger.error("Video menu: {}",error.what());}
}
std::string Duration(int seconds){return std::to_string(seconds/60)+":"+(seconds%60<10?"0":"")+std::to_string(seconds%60);}
}
void Theater::VideoMenuView::ctor(){INVOKE_CTOR();UnityEngine::MonoBehaviour::_ctor();page=0;selectedIndex=-1;shownRevision=std::numeric_limits<uint64_t>::max();nextRefresh=0;searchOpen=false;initialized=false;levelID="";}
void Theater::VideoMenuView::OnDisable(){theater::StopMenuPreview();ClearImages(this);shownRevision=std::numeric_limits<uint64_t>::max();}
void Theater::VideoMenuView::OnDestroy(){theater::StopMenuPreview();ClearImages(this);}
void Theater::VideoMenuView::Update() {
    if(!initialized)return;
    theater::UpdateMenuPreview();
    auto& pending=images[this].pending;
    for(auto it=pending.begin();it!=pending.end();) {
        if(!it->request->get_isDone()){++it;continue;}
        if(it->request->get_result().value__==1) {
            auto texture=Networking::DownloadHandlerTexture::GetContent(it->request).ptr();
            if(texture) {
                images[this].textures.emplace_back(texture,il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(texture),false));
                thumbnails[it->row]->set_texture(texture);thumbnails[it->row]->set_color({1,1,1,1});
            }
        }
        it->request->Dispose();il2cpp_functions::gchandle_free(it->handle);it=pending.erase(it);
    }
    float now=Time::get_realtimeSinceStartup();if(now<nextRefresh)return;nextRefresh=now+.25f;
    try {
        auto info=theater::SelectedVideo();auto download=theater::SelectedDownload();auto search=theater::SearchResults();
        if(static_cast<std::string>(levelID)!=info.levelID) {
            levelID=info.levelID;queryInput->set_text(info.query);page=0;selectedIndex=-1;searchOpen=false;
            shownRevision=std::numeric_limits<uint64_t>::max();
        }
        detailsRoot->SetActive(!searchOpen);searchRoot->SetActive(searchOpen);
        titleLabel->set_text(info.config?info.config->title:"No video configured");
        metadataLabel->set_text(info.config?info.config->author+"   "+Duration(info.config->duration):"Select a song and search YouTube");
        auto status=download.text;
        if(info.config&&!info.playbackAllowed)status+="\nThis difficulty has no Theater/Cinema suggestion";
        statusLabel->set_text(status);
        downloadButton->set_interactable(download.configured&&!download.ready&&!download.busy);
        deleteButton->set_interactable(download.ready&&!download.busy);
        deleteConfigButton->set_interactable(info.config.has_value()&&!download.busy);
        previewButton->set_interactable(download.ready&&!download.busy);
        BSML::Lite::SetButtonText(previewButton,theater::MenuPreviewActive()?"Stop preview":"Preview");
        offsetRoot->SetActive(info.config.has_value());
        offsetLabel->set_text(info.config?std::to_string(static_cast<int>(std::llround(info.config->offset*1000)))+" ms":"");
        auto previewStatus=theater::MenuPreviewStatus();
        if(theater::MenuPreviewActive()||previewStatus.starts_with("Preview failed:"))previewLabel->set_text(previewStatus);
        if(search.revision!=shownRevision) {
            shownRevision=search.revision;ClearImages(this);selectedIndex=-1;
            searchLabel->set_text(search.message);
            if(page*4>=static_cast<int>(search.results.size()))page=0;
            for(int row=0;row<4;row++) {
                int index=page*4+row;bool exists=index<static_cast<int>(search.results.size());
                resultButtons[row]->get_transform()->get_parent()->get_gameObject()->SetActive(exists);
                if(!exists)continue;
                auto& video=search.results[index];
                BSML::Lite::SetButtonText(resultButtons[row],video.title+"\n"+video.author+" · "+Duration(video.duration));
                auto label=resultButtons[row]->GetComponentInChildren<TMPro::TextMeshProUGUI*>();
                if(label){label->set_richText(false);label->set_fontSize(2.8f);label->set_enableWordWrapping(true);label->set_overflowMode(TMPro::TextOverflowModes::Ellipsis);}
                thumbnails[row]->set_texture(nullptr);thumbnails[row]->set_color({.15f,.15f,.15f,1});
                if(video.thumbnail.starts_with("https://")) {
                    auto request=Networking::UnityWebRequestTexture::GetTexture(video.thumbnail,true);request->set_timeout(15);request->SendWebRequest();
                    images[this].pending.push_back({request,il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(request),false),row});
                }
            }
        }
        resultDownloadButton->set_interactable(selectedIndex>=0&&!search.busy&&!download.busy&&!download.ready);
    }catch(const std::exception& error){previewLabel->set_text(error.what());}
}
void CreateVideoMenu(UnityEngine::Transform* parent) {
    auto view=parent->get_gameObject()->AddComponent<Theater::VideoMenuView*>();
    auto details=BSML::Lite::CreateVerticalLayoutGroup(parent);details->set_spacing(0);view->detailsRoot=details->get_gameObject();
    auto transform=details->get_transform();
    view->titleLabel=Label(transform,"No video configured",10,4);view->metadataLabel=Label(transform,"",6);
    view->statusLabel=Label(transform,"",8);
    auto downloads=Row(transform);
    view->downloadButton=BSML::Lite::CreateUIButton(downloads,"Download Video",[view]{Action(view,[]{theater::RetrySelectedDownload();});});Size(view->downloadButton,48,8);
    view->deleteButton=BSML::Lite::CreateUIButton(downloads,"Delete Video",[view]{Action(view,[]{theater::DeleteSelectedVideo();});});Size(view->deleteButton,28,8);
    auto offset=BSML::Lite::CreateVerticalLayoutGroup(transform);offset->set_spacing(0);view->offsetRoot=offset->get_gameObject();
    Label(offset->get_transform(),"Video Offset",4);
    auto controls=Row(offset->get_transform());
    for(int delta:{-1000,-100,-20}) {
        std::string text=delta==-1000?"---":delta==-100?"--":"-";
        auto button=BSML::Lite::CreateUIButton(controls,text,[view,delta]{Action(view,[delta]{theater::ChangeSelectedOffset(delta);});});Size(button,9,8);
    }
    view->offsetLabel=Label(controls->get_transform(),"0 ms",8);Size(view->offsetLabel,19,8);view->offsetLabel->set_alignment(TMPro::TextAlignmentOptions::Center);
    for(int delta:{20,100,1000}) {
        std::string text=delta==1000?"+++":delta==100?"++":"+";
        auto button=BSML::Lite::CreateUIButton(controls,text,[view,delta]{Action(view,[delta]{theater::ChangeSelectedOffset(delta);});});Size(button,9,8);
    }
    auto actions=Row(transform);
    view->previewButton=BSML::Lite::CreateUIButton(actions,"Preview",[view]{Action(view,[]{theater::ToggleMenuPreview();});});Size(view->previewButton,24,8);
    auto refine=BSML::Lite::CreateUIButton(actions,"Search",[view]{view->searchOpen=true;view->nextRefresh=0;});Size(refine,20,8);
    view->deleteConfigButton=BSML::Lite::CreateUIButton(actions,"Delete config",[view]{Action(view,[]{theater::DeleteSelectedConfig();});});Size(view->deleteConfigButton,31,8);
    view->previewLabel=Label(transform,"",6,2.5f);
    auto search=BSML::Lite::CreateVerticalLayoutGroup(parent);search->set_spacing(0);view->searchRoot=search->get_gameObject();
    view->queryInput=BSML::Lite::CreateStringSetting(search,"YouTube","");
    auto searchActions=Row(search->get_transform());
    auto query=BSML::Lite::CreateUIButton(searchActions,"Search",[view]{Action(view,[view]{view->page=0;theater::SearchVideos(static_cast<std::string>(view->queryInput->get_text()));});});Size(query,40,8);
    auto back=BSML::Lite::CreateUIButton(searchActions,"Go Back",[view]{view->searchOpen=false;view->nextRefresh=0;});Size(back,36,8);
    view->searchLabel=Label(search->get_transform(),"Search YouTube for the selected song",6,2.5f);
    view->resultButtons=ArrayW<UI::Button*>(4);view->thumbnails=ArrayW<UI::RawImage*>(4);
    for(int i=0;i<4;i++) {
        auto row=Row(search->get_transform());Size(row,78,12);
        view->thumbnails[i]=Thumbnail(row->get_transform());
        view->resultButtons[i]=BSML::Lite::CreateUIButton(row,"",[view,i] {
            view->selectedIndex=view->page*4+i;auto results=theater::SearchResults();
            if(view->selectedIndex<static_cast<int>(results.results.size()))view->searchLabel->set_text("Selected: "+results.results[view->selectedIndex].title);
            view->nextRefresh=0;
        });Size(view->resultButtons[i],58,11);
    }
    auto pages=Row(search->get_transform());
    auto previous=BSML::Lite::CreateUIButton(pages,"<",[view]{view->page=std::max(0,view->page-1);view->shownRevision=std::numeric_limits<uint64_t>::max();view->nextRefresh=0;});Size(previous,17,8);
    view->resultDownloadButton=BSML::Lite::CreateUIButton(pages,"Download",[view]{Action(view,[view] {
        theater::ChooseSearchVideo(view->selectedIndex);theater::RetrySelectedDownload();view->searchOpen=false;
    });});Size(view->resultDownloadButton,40,8);
    auto next=BSML::Lite::CreateUIButton(pages,">",[view]{auto results=theater::SearchResults();if((view->page+1)*4<static_cast<int>(results.results.size()))view->page++;view->shownRevision=std::numeric_limits<uint64_t>::max();view->nextRefresh=0;});Size(next,17,8);
    view->searchRoot->SetActive(false);
    view->initialized=true;
    PaperLogger.info("Theater video menu initialized with four RawImage thumbnails");
}
