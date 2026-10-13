#include "video_menu.hpp"
#include "downloads.hpp"
#include "preview.hpp"
#include "main.hpp"
#include "modconfig.hpp"
#include "video_menu_layout.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/Components/ButtonIconImage.hpp"
#include "UnityEngine/RectTransform.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/Image.hpp"
#include "UnityEngine/Networking/UnityWebRequestTexture.hpp"
#include "UnityEngine/Networking/UnityWebRequest.hpp"
#include "UnityEngine/Networking/DownloadHandlerTexture.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Time.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/IPreviewMediaData.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include <unordered_map>
#include <limits>
#include <regex>
#include <cmath>
#include <vector>
#include "theater_trash_icon.inc"
DEFINE_TYPE(Theater, VideoMenuView);
namespace {
using namespace UnityEngine;
using View=Theater::VideoMenuView;
struct Images {
    struct Pending {Networking::UnityWebRequest* request;uint32_t handle;int index;};
    std::vector<Pending> pending;
    std::vector<std::pair<Object*,uint32_t>> owned;
};
struct UIState {
    std::shared_ptr<BSML::BSMLParser> parser;
    Images detail,results;
    std::string thumbnail;
    System::Threading::Tasks::Task_1<UnityW<Sprite>>* cover{};
    uint32_t coverHandle{};
};
std::unordered_map<View*,UIState> views;
void Clear(Images& images) {
    for(auto& item:images.pending){item.request->Abort();item.request->Dispose();il2cpp_functions::gchandle_free(item.handle);}
    for(auto [object,handle]:images.owned){if(Object::op_Implicit_bool(object))Object::Destroy(object);il2cpp_functions::gchandle_free(handle);}
    images=Images{};
}
void ClearDetail(View* view,UIState& state) {
    if(view->detailsThumbnail&&Object::op_Implicit_bool(view->detailsThumbnail))view->detailsThumbnail->set_sprite(nullptr);
    Clear(state.detail);state.thumbnail.clear();state.cover=nullptr;
    if(state.coverHandle){il2cpp_functions::gchandle_free(state.coverHandle);state.coverHandle=0;}
}
void ClearResults(View* view,UIState& state) {
    if(view->resultsList)for(auto cell:view->resultsList->data)cell->icon=nullptr;
    Clear(state.results);
}
void Fetch(Images& images,const std::string& url,int index) {
    auto request=Networking::UnityWebRequestTexture::GetTexture(url,true);
    request->set_timeout(15);request->SendWebRequest();
    images.pending.push_back({request,il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(request),false),index});
}
void Poll(View* view,Images& images,bool detail) {
    bool updated=false;
    for(auto it=images.pending.begin();it!=images.pending.end();) {
        if(!it->request->get_isDone()){++it;continue;}
        try {
            if(it->request->get_result().value__==1) {
                auto texture=Networking::DownloadHandlerTexture::GetContent(it->request).ptr();
                if(texture) {
                    images.owned.emplace_back(texture,il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(texture),false));
                    auto sprite=BSML::Lite::TextureToSprite(texture);
                    images.owned.emplace_back(sprite,il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(sprite),false));
                    if(detail)view->detailsThumbnail->set_sprite(sprite);
                    else if(it->index<view->resultsList->data->get_Count()){view->resultsList->data[it->index]->icon=sprite;updated=true;}
                }
            }
        }catch(const std::exception& error){PaperLogger.warn("Theater thumbnail: {}",error.what());}
        it->request->Dispose();il2cpp_functions::gchandle_free(it->handle);it=images.pending.erase(it);
    }
    if(updated)view->resultsList->tableView->ReloadDataKeepingPosition();
}
std::string Duration(int seconds){return std::to_string(seconds/60)+":"+(seconds%60<10?"0":"")+std::to_string(seconds%60);}
std::string Plain(std::string value) {
    std::replace(value.begin(),value.end(),'<','(');std::replace(value.begin(),value.end(),'>',')');return value;
}
std::string ThumbnailURL(const std::string& source) {
    std::smatch match;
    if(std::regex_search(source,match,std::regex(R"((?:[?&]v=|youtu\.be/|/shorts/|/embed/)([A-Za-z0-9_-]{11}))")))
        return "https://i.ytimg.com/vi/"+match[1].str()+"/hqdefault.jpg";
    return {};
}
void UpdateThumbnail(View* view,UIState& state,const theater::SelectedVideoInfo& info) {
    auto url=ThumbnailURL(info.config->sourceUrl),key=url.empty()?info.levelID:url;
    if(state.thumbnail==key)return;
    ClearDetail(view,state);state.thumbnail=key;
    if(!url.empty())Fetch(state.detail,url,-1);
    else if(auto level=theater::SelectedLevel();level&&level->previewMediaData) {
        state.cover=level->previewMediaData->GetCoverSpriteAsync();
        state.coverHandle=il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(state.cover),false);
    }
}
void Action(View* view,const std::function<void()>& action) {
    try{action();view->nextRefresh=0;}
    catch(const std::exception& error){auto label=view->searchOpen?view->searchLabel:view->statusLabel;label->set_text(error.what());label->set_color({1,0,0,1});PaperLogger.error("Video menu: {}",error.what());}
}
void Offset(View* view,int delta){Action(view,[delta]{theater::ChangeSelectedOffset(delta);});}
void Underline(UI::Button* button,Color color) {
    auto child=button->get_transform()->Find("Underline");
    if(child)if(auto image=child->GetComponent<UI::Image*>())image->set_color(color);
}
}
void Theater::VideoMenuView::ctor(){
    INVOKE_CTOR();UnityEngine::MonoBehaviour::_ctor();
    selectedIndex=-1;shownRevision=std::numeric_limits<uint64_t>::max();nextRefresh=0;
    searchOpen=false;initialized=false;customizeOffset=false;levelID="";query="";
}
void Theater::VideoMenuView::OnDisable(){
    theater::StopMenuPreview();
    auto found=views.find(this);if(found==views.end())return;
    found->second.parser->parserParams->EmitEvent("hide-keyboard");
    ClearDetail(this,found->second);ClearResults(this,found->second);
    shownRevision=std::numeric_limits<uint64_t>::max();
}
void Theater::VideoMenuView::OnDestroy(){OnDisable();views.erase(this);}
void Theater::VideoMenuView::Update() {
    if(!initialized)return;
    try {
        theater::UpdateMenuPreview();auto& state=views.at(this);
        Poll(this,state.detail,true);Poll(this,state.results,false);
        if(state.cover&&state.cover->get_IsCompleted()) {
            if(!state.cover->get_IsFaulted()&&!state.cover->get_IsCanceled())detailsThumbnail->set_sprite(state.cover->get_Result());
            state.cover=nullptr;il2cpp_functions::gchandle_free(state.coverHandle);state.coverHandle=0;
        }
        float now=Time::get_realtimeSinceStartup();if(now<nextRefresh)return;nextRefresh=now+.25f;
        auto info=theater::SelectedVideo();auto download=theater::SelectedDownload();auto search=theater::SearchResults();
        if(static_cast<std::string>(levelID)!=info.levelID) {
            levelID=info.levelID;query=info.query;selectedIndex=-1;searchOpen=false;
            customizeOffset=info.config&&(!info.config->configByMapper||info.config->customOffset);
            ClearDetail(this,state);ClearResults(this,state);shownRevision=std::numeric_limits<uint64_t>::max();
            state.parser->parserParams->EmitEvent("hide-keyboard");
        }
        bool configured=info.config.has_value()&&getModConfig().Enabled.GetValue();
        noVideoRoot->SetActive(!searchOpen&&!configured);
        detailsRoot->SetActive(!searchOpen&&configured);searchRoot->SetActive(searchOpen);
        noVideoLabel->set_text(!getModConfig().Enabled.GetValue()?"Theater is disabled.\nYou can re-enable it on the left side of the main menu.":info.levelID.empty()?"No level selected":"No video configured");
        searchButton->get_gameObject()->SetActive(!info.levelID.empty()&&getModConfig().Enabled.GetValue());
        if(configured) {
            titleLabel->set_text(info.config->title.empty()?"Untitled Video":info.config->title);
            authorLabel->set_text("Author: "+(info.config->author.empty()?"Unknown Author":info.config->author));
            durationLabel->set_text("Duration: "+Duration(info.config->duration));
            statusLabel->set_text(download.ready?"Downloaded":download.busy?download.text:download.text.starts_with("Failed:")?download.text.substr(7):"Not downloaded");
            statusLabel->set_color(download.ready?Color{0,1,0,1}:download.busy?Color{1,1,0,1}:Color{1,0,0,1});
            BSML::Lite::SetButtonText(mediaButton,download.ready?"Delete Video":download.busy?"Cancel":"Download");
            mediaButton->set_interactable(true);Underline(mediaButton,download.ready||download.busy?Color{.5f,.5f,.5f,1}:Color{0,1,0,1});
            deleteConfigButton->get_gameObject()->SetActive(!info.config->configByMapper);
            deleteConfigButton->set_interactable(!download.busy);
            previewButton->set_interactable(download.ready&&!download.busy);
            BSML::Lite::SetButtonText(previewButton,theater::MenuPreviewActive()?"Stop preview":"Preview");
            if(info.config->configByMapper&&customizeOffset!=info.config->customOffset){theater::SetSelectedCustomOffset(customizeOffset);info=theater::SelectedVideo();}
            bool editable=!info.config->configByMapper||info.config->customOffset;
            customizeOffset=editable;customizeToggle->set_Value(editable);
            offsetRoot->SetActive(editable);customizeOffsetRoot->SetActive(info.config->configByMapper);
            offsetLabel->set_text(std::to_string(static_cast<int>(std::llround(info.config->offset*1000)))+" ms");
            if(!searchOpen)UpdateThumbnail(this,state,info);
            auto preview=theater::MenuPreviewStatus();
            if(preview.starts_with("Preview failed:")){statusLabel->set_text(preview);statusLabel->set_color({1,0,0,1});}
        }
        if(search.revision!=shownRevision) {
            shownRevision=search.revision;ClearResults(this,state);selectedIndex=-1;
            resultsList->data->Clear();
            for(size_t i=0;i<search.results.size();i++) {
                const auto& video=search.results[i];
                resultsList->data->Add(BSML::CustomCellInfo::construct("["+Duration(video.duration)+"] "+Plain(video.title),Plain(video.author)));
                if(video.thumbnail.starts_with("https://"))Fetch(state.results,video.thumbnail,static_cast<int>(i));
            }
            resultsList->tableView->ReloadData();resultsList->tableView->ClearSelection();
        }
        searchLabel->get_gameObject()->SetActive(search.busy||search.results.empty());
        searchLabel->set_text(search.busy?"Searching for videos, please wait...":search.message.starts_with("Search failed:")?search.message:"No search results found.\nUse the Refine Search button in the bottom right to choose a different search query.");
        resultDownloadButton->set_interactable(selectedIndex>=0&&!search.busy&&!download.busy&&!download.ready);
        Underline(resultDownloadButton,selectedIndex>=0?Color{0,1,0,1}:Color{.5f,.5f,.5f,1});
    }catch(const std::exception& error){statusLabel->set_text(error.what());PaperLogger.error("Video menu update: {}",error.what());}
}
void Theater::VideoMenuView::SearchClick(){QueryEntered(query);}
void Theater::VideoMenuView::QueryEntered(StringW text){Action(this,[&]{query=text;theater::SearchVideos(static_cast<std::string>(text));searchOpen=true;views.at(this).parser->parserParams->EmitEvent("hide-keyboard");});}
void Theater::VideoMenuView::RefineClick(){queryKeyboard->SetText(query);views.at(this).parser->parserParams->EmitEvent("show-keyboard");}
void Theater::VideoMenuView::BackClick(){searchOpen=false;nextRefresh=0;}
void Theater::VideoMenuView::MediaClick(){Action(this,[]{auto state=theater::SelectedDownload();if(state.busy)theater::CancelDownloads();else if(state.ready)theater::DeleteSelectedVideo();else theater::RetrySelectedDownload();});}
void Theater::VideoMenuView::DeleteConfigClick(){Action(this,[]{theater::DeleteSelectedConfig();});}
void Theater::VideoMenuView::PreviewClick(){Action(this,[]{theater::ToggleMenuPreview();});}
void Theater::VideoMenuView::SelectCell(HMUI::TableView*,int index){selectedIndex=index;nextRefresh=0;}
void Theater::VideoMenuView::DownloadClick(){Action(this,[&]{theater::ChooseSearchVideo(selectedIndex);theater::RetrySelectedDownload();searchOpen=false;});}
void Theater::VideoMenuView::OffsetMinusHigh(){Offset(this,-1000);}
void Theater::VideoMenuView::OffsetMinusMid(){Offset(this,-100);}
void Theater::VideoMenuView::OffsetMinusLow(){Offset(this,-20);}
void Theater::VideoMenuView::OffsetPlusHigh(){Offset(this,1000);}
void Theater::VideoMenuView::OffsetPlusMid(){Offset(this,100);}
void Theater::VideoMenuView::OffsetPlusLow(){Offset(this,20);}
void CreateVideoMenu(UnityEngine::Transform* parent) {
    auto view=parent->get_gameObject()->AddComponent<Theater::VideoMenuView*>();
    auto& state=views[view];state.parser=BSML::parse_and_construct(TheaterVideoMenuBSML,parent,view);
    if(!view->menuRoot||!view->noVideoRoot||!view->detailsRoot||!view->searchRoot||!view->titleLabel||!view->detailsThumbnail||
       !view->authorLabel||!view->durationLabel||!view->statusLabel||!view->offsetLabel||!view->previewButton||
       !view->mediaButton||!view->deleteConfigButton||!view->resultsList||!view->queryKeyboard||!view->customizeToggle)
        throw std::runtime_error("Original Theater menu binding failed");
    auto icon=view->deleteConfigButton->GetComponent<BSML::ButtonIconImage*>();
    icon->SetIcon(BSML::Lite::VectorToSprite(theaterTrashIconBytes));
    for(auto label:{view->titleLabel,view->authorLabel,view->durationLabel,view->statusLabel,view->offsetLabel,view->noVideoLabel,view->searchLabel})label->set_richText(false);
    for(auto label:{view->titleLabel,view->authorLabel,view->durationLabel,view->statusLabel,view->offsetLabel})label->set_enableWordWrapping(false);
    // Background has no layout component on Quest: keep its specified 30-unit height.
    view->noVideoRoot->GetComponent<UI::ContentSizeFitter*>()->set_verticalFit(UI::ContentSizeFitter::FitMode::Unconstrained);
    view->searchRoot->SetActive(false);view->detailsRoot->SetActive(false);
    view->initialized=true;view->Update();
    PaperLogger.info("Original Theater menu initialized: PC details, thumbnail, offsets and six-row search");
}
