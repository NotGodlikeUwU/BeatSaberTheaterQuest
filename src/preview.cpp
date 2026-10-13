#include "preview.hpp"
#include "downloads.hpp"
#include "main.hpp"
#include "modconfig.hpp"
#include "video_processing.hpp"
#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "GlobalNamespace/LevelCollectionViewController.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/BeatmapLevelDataVersion.hpp"
#include "GlobalNamespace/LoadBeatmapLevelDataResult.hpp"
#include "GlobalNamespace/AudioClipAsyncLoaderExtensions.hpp"
#include "GlobalNamespace/SongPreviewPlayer.hpp"
#include "System/Threading/CancellationTokenSource.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/Rendering/ShadowCastingMode.hpp"
#include "UnityEngine/Collider.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Video/VideoPlayer.hpp"
#include "UnityEngine/Video/VideoSource.hpp"
#include "UnityEngine/Video/VideoRenderMode.hpp"
#include "UnityEngine/Video/VideoAudioOutputMode.hpp"
#include <cmath>
#include <algorithm>
namespace theater {
namespace {
using namespace UnityEngine;
struct Preview {
    bool active{}, started{};
    int frame{-1};
    std::string id,message{"Preview stopped"};
    float began{},lastSeek{};
    double offset{};
    std::optional<VideoConfig> config;
    GameObject* root{}, *screen{};
    Video::VideoPlayer* player{};
    Material* material{};
    Mesh* mesh{};
    GlobalNamespace::SongPreviewPlayer* music{};
    GlobalNamespace::AudioClipAsyncLoader* loader{};
    GlobalNamespace::IBeatmapLevelData* data{};
    AudioClip* clip{};
    AudioSource* reference{},*videoAudio{};
    Texture* display{};
    std::unique_ptr<VideoImageProcessor> processor;
    System::Threading::CancellationTokenSource* cancel{};
    System::Threading::Tasks::Task_1<GlobalNamespace::LoadBeatmapLevelDataResult>* levelTask{};
    System::Threading::Tasks::Task_1<UnityW<AudioClip>>* clipTask{};
    std::vector<uint32_t> handles;
    std::vector<std::pair<AudioSource*,float>> pans;
} state;
template<typename T>T* Hold(T* object) {if(object)state.handles.push_back(il2cpp_functions::gchandle_new(reinterpret_cast<Il2CppObject*>(object),false));return object;}
template<typename F>void CleanupStep(F action) {
    try{action();}catch(const std::exception& error){PaperLogger.warn("Preview cleanup: {}",error.what());}
}
}
void StopMenuPreview() {
    bool wasActive=state.active;
    state.active=false;
    CleanupStep([]{if(state.cancel)state.cancel->Cancel();});
    CleanupStep([]{if(state.player&&Object::op_Implicit_bool(state.player))state.player->Stop();});
    for(auto [source,pan]:state.pans)CleanupStep([&]{if(Object::op_Implicit_bool(source))source->set_panStereo(pan);});
    CleanupStep([&]{if(wasActive&&state.music&&Object::op_Implicit_bool(state.music))state.music->CrossfadeToDefault();});
    CleanupStep([]{if(state.data&&state.loader)GlobalNamespace::AudioClipAsyncLoaderExtensions::UnloadSong(state.loader,state.data);});
    CleanupStep([]{if(state.root&&Object::op_Implicit_bool(state.root)){state.root->SetActive(false);Object::Destroy(state.root);}});
    state.processor.reset();
    CleanupStep([]{if(state.material&&Object::op_Implicit_bool(state.material))Object::Destroy(state.material);});
    CleanupStep([]{if(state.mesh&&Object::op_Implicit_bool(state.mesh))Object::Destroy(state.mesh);});
    CleanupStep([]{if(state.cancel)state.cancel->Dispose();});
    for(auto handle:state.handles)il2cpp_functions::gchandle_free(handle);
    state=Preview{};
}
void ToggleMenuPreview() {
    if(state.active){StopMenuPreview();return;}
    auto info=SelectedVideo();
    if(!info.config||!VideoAvailable(*info.config)||info.busy)return;
    StopMenuPreview();
    try {
        GlobalNamespace::BeatmapLevelsModel* model=nullptr;
        for(auto controller:Resources::FindObjectsOfTypeAll<GlobalNamespace::StandardLevelDetailViewController*>())
            if(controller->_beatmapLevelsModel)model=controller->_beatmapLevelsModel;
        for(auto controller:Resources::FindObjectsOfTypeAll<GlobalNamespace::LevelCollectionViewController*>())
            if(controller->_audioClipAsyncLoader)state.loader=controller->_audioClipAsyncLoader;
        for(auto player:Resources::FindObjectsOfTypeAll<GlobalNamespace::SongPreviewPlayer*>())
            if(player->get_isActiveAndEnabled())state.music=player;
        if(!model||!state.loader||!state.music)throw std::runtime_error("Song preview services are not ready");
        Hold(model);Hold(state.loader);Hold(state.music);
        state.cancel=Hold(System::Threading::CancellationTokenSource::New_ctor());
        state.levelTask=Hold(model->LoadBeatmapLevelDataAsync(info.levelID,GlobalNamespace::BeatmapLevelDataVersion::Original,state.cancel->get_Token()));
        state.id=info.levelID;state.config=info.config;state.offset=info.config->offset;
        state.root=Hold(GameObject::New_ctor("TheaterMenuPreview"));
        state.videoAudio=Hold(state.root->AddComponent<AudioSource*>());
        state.videoAudio->set_playOnAwake(false);state.videoAudio->set_spatialBlend(0);state.videoAudio->set_panStereo(0);state.videoAudio->set_volume(.8f);
        state.player=Hold(state.root->AddComponent<Video::VideoPlayer*>());
        state.player->set_playOnAwake(false);state.player->set_source(Video::VideoSource::Url);
        state.player->set_renderMode(Video::VideoRenderMode::APIOnly);
        state.player->set_audioOutputMode(Video::VideoAudioOutputMode::AudioSource);
        state.player->set_controlledAudioTrackCount(1);state.player->SetTargetAudioSource(0,state.videoAudio);
        state.player->set_waitForFirstFrame(true);state.player->set_skipOnDrop(true);state.player->set_isLooping(info.config->loop);
        state.player->set_url(info.config->file.string());state.player->Prepare();
        state.active=true;state.began=Time::get_realtimeSinceStartup();state.message="Preparing preview...";
    }catch(const std::exception& error){StopMenuPreview();state.message=std::string("Preview failed: ")+error.what();PaperLogger.error("{}",state.message);}
}
void UpdateMenuPreview() {
    if(!state.active||state.frame==Time::get_frameCount())return;
    state.frame=Time::get_frameCount();
    try {
        auto info=SelectedVideo();
        if(info.levelID!=state.id||!info.config){StopMenuPreview();return;}
        state.config=info.config;
        float now=Time::get_realtimeSinceStartup();
        if(!state.started&&now-state.began>std::clamp(getModConfig().PrepareTimeout.GetValue(),5.f,120.f))throw std::runtime_error("Preview preparation timed out");
        if(state.levelTask) {
            if(!state.levelTask->get_IsCompleted())return;
            if(state.levelTask->get_IsFaulted()||state.levelTask->get_IsCanceled())throw std::runtime_error("Map audio data could not be loaded");
            auto result=state.levelTask->get_Result();state.levelTask=nullptr;
            if(result.isError||!result.beatmapLevelData)throw std::runtime_error("Map audio is unavailable (DLC must be owned and installed)");
            state.data=Hold(result.beatmapLevelData);
            state.clipTask=Hold(GlobalNamespace::AudioClipAsyncLoaderExtensions::LoadSong(state.loader,state.data));
        }
        if(state.clipTask) {
            if(!state.clipTask->get_IsCompleted())return;
            if(state.clipTask->get_IsFaulted()||state.clipTask->get_IsCanceled())throw std::runtime_error("Map audio clip could not be loaded");
            auto clip=state.clipTask->get_Result();state.clipTask=nullptr;
            if(!clip)throw std::runtime_error("Map audio loader returned an empty clip");
            state.clip=Hold(clip.ptr());
            PaperLogger.info("Preview map audio loaded: {}",state.id);
        }
        if(!state.clip||!state.player->get_isPrepared())return;
        if(!state.started) {
            float start=std::max(0.0,-state.config->offset/state.config->speed);
            if(start>=state.clip->get_length())throw std::runtime_error("Video offset starts after the end of this song");
            state.music->CrossfadeTo(state.clip,-5.f,start,state.clip->get_length()-start,nullptr);
            for(auto controller:state.music->_audioSourceControllers) {
                if(!controller||!controller->audioSource)continue;
                auto source=controller->audioSource.ptr();
                state.pans.emplace_back(Hold(source),source->get_panStereo());
                auto clip=source->get_clip();
                if(clip&&clip.ptr()==state.clip)source->set_panStereo(0);
            }
            if(state.player->get_audioTrackCount()>0)state.player->EnableAudioTrack(0,true);
            state.screen=Hold(GameObject::CreatePrimitive(PrimitiveType::Quad).ptr());
            state.screen->set_name("TheaterMenuScreen");state.screen->get_transform()->SetParent(state.root->get_transform(),false);
            state.screen->get_transform()->set_position({0,4,16});
            float aspect=state.player->get_height()?static_cast<float>(state.player->get_width())/state.player->get_height():16.f/9.f;
            state.screen->get_transform()->set_localScale({8*aspect,8,1});
            if(auto collider=state.screen->GetComponent<Collider*>())Object::Destroy(collider);
            state.material=Hold(CreateVideoMaterial(*state.config,true));auto renderer=state.screen->GetComponent<MeshRenderer*>();renderer->set_sharedMaterial(state.material);
            state.mesh=Hold(state.screen->GetComponent<MeshFilter*>()->get_mesh().ptr());
            ArrayW<Color> colors(state.mesh->get_vertexCount());for(auto& color:colors)color={1,1,1,1};state.mesh->set_colors(colors);
            renderer->set_shadowCastingMode(Rendering::ShadowCastingMode::Off);renderer->set_receiveShadows(false);
            state.processor=std::make_unique<VideoImageProcessor>(*state.config);
            state.player->set_time(std::max(0.0,ReferenceTime(start,*state.config,state.player->get_length())));
            state.player->set_playbackSpeed(state.config->speed);state.player->Play();state.started=true;state.lastSeek=now;
            state.message=state.player->get_audioTrackCount()?"Preview: stereo audio":"Preview: this cached video has no audio track";
            PaperLogger.info("Preview started: {}",state.id);
        }
        AudioSource* reference=nullptr;
        for(auto controller:state.music->_audioSourceControllers) {
            if(!controller||!controller->audioSource)continue;
            auto source=controller->audioSource.ptr();auto clip=source->get_clip();
            if(clip&&clip.ptr()==state.clip&&source->get_isPlaying())reference=source;
        }
        if(!reference){if(now-state.lastSeek>2)StopMenuPreview();return;}
        reference->set_panStereo(0);state.reference=reference;
        auto target=ReferenceTime(reference->get_time(),*state.config,state.player->get_length());
        bool visible=target>=0&&(state.config->end<0||target<state.config->end)&&(state.config->loop||target<state.player->get_length());
        state.screen->SetActive(visible);
        if(!visible){state.player->Pause();return;}
        if(state.offset!=state.config->offset||(std::abs(state.player->get_time()-target)>.3&&now-state.lastSeek>1)) {
            if(state.player->get_canSetTime())state.player->set_time(target);
            state.offset=state.config->offset;state.lastSeek=now;
        }
        if(!state.player->get_isPlaying())state.player->Play();
        auto texture=state.player->get_texture();
        if(!texture)return; // Prepared can precede the first decoded video frame.
        state.display=state.processor->Update(texture.ptr());
        state.material->set_mainTexture(state.display);
    }catch(const std::exception& error){StopMenuPreview();state.message=std::string("Preview failed: ")+error.what();PaperLogger.error("{}",state.message);}
}
bool MenuPreviewActive(){return state.active;}
void MenuPreviewError(UnityEngine::Video::VideoPlayer* player,const std::string& error) {
    if(player!=state.player)return;
    StopMenuPreview();state.message="Preview failed: "+error;PaperLogger.error("{}",state.message);
}
std::string MenuPreviewStatus(){return state.message;}
UnityEngine::Texture* MenuPreviewTexture(){return state.display;}
}
