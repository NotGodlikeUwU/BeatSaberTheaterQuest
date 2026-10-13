#include "main.hpp"
#include "modconfig.hpp"
#include "video_config.hpp"
#include "downloads.hpp"
#include "screen_effects.hpp"
#include "UnityEngine/LayerMask.hpp"
#include "GlobalNamespace/GameplayCoreInstaller.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/MeshFilter.hpp"
#include "UnityEngine/MeshRenderer.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Video/VideoPlayer.hpp"
#include "UnityEngine/Video/VideoRenderMode.hpp"
#include "UnityEngine/Video/VideoAudioOutputMode.hpp"
#include "UnityEngine/Video/VideoSource.hpp"
#include "songcore/shared/SongCore.hpp"
#include "songcore/shared/SongLoader/CustomBeatmapLevel.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

using namespace UnityEngine;
using GlobalNamespace::AudioTimeSyncController;
void RegisterTheaterSettings();
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};
namespace {
std::optional<theater::VideoConfig> selected;
GameObject* root{};
Video::VideoPlayer* player{};
Material* material{};
Mesh* mesh{};
AudioTimeSyncController* owner{};
bool paused{}, started{}, seekPending{};
float prepareStarted{}, lastSeek{};
std::vector<GameObject*> screens;

void Cleanup() {
    theater::StopScreenEffects();
    if (player && Object::op_Implicit_bool(player)) player->Stop();
    if (root && Object::op_Implicit_bool(root)) Object::Destroy(root);
    if (material && Object::op_Implicit_bool(material)) Object::Destroy(material);
    if (mesh && Object::op_Implicit_bool(mesh)) Object::Destroy(mesh);
    root = nullptr; player = nullptr; material = nullptr; mesh = nullptr;
    owner = nullptr; screens.clear(); started = false; paused = false; seekPending = false;
}
void Visibility(bool visible) {
    for (auto screen : screens) screen->SetActive(visible);
    theater::UpdateScreenLight(player ? player->get_texture() : nullptr,visible);
}
void PausedScreenEffects() {
    bool visible=!screens.empty() && screens.front()->get_activeSelf();
    theater::UpdateScreenLight(player ? player->get_texture() : nullptr,visible);
}
Mesh* BuildMesh(float aspect) {
    auto& c = *selected;
    int n = c.curvature == 0 ? 1 : c.segments;
    ArrayW<Vector3> vertices((n + 1) * 2);
    ArrayW<Vector2> uv((n + 1) * 2);
    ArrayW<int> triangles(n * 6);
    float width = c.height * aspect;
    float angle = c.curvature * 3.14159265359f / 180.0f;
    float axisLength = c.curveYAxis ? c.height : width;
    float radius = angle == 0 ? 0 : axisLength / angle;
    for (int i = 0; i <= n; ++i) {
        float u = static_cast<float>(i) / n;
        float along = axisLength * (u - .5f), z = 0;
        if (angle != 0) { along = std::sin(angle * (u - .5f)) * radius; z = (std::cos(angle * (u - .5f)) - 1) * radius; }
        for (int j = 0; j < 2; ++j) {
            vertices[i * 2 + j] = c.curveYAxis ? Vector3((j - .5f) * width, along, z) : Vector3(along, (j - .5f) * c.height, z);
            uv[i * 2 + j] = c.curveYAxis ? Vector2(static_cast<float>(j),u) : Vector2(u,static_cast<float>(j));
        }
        if (i < n) {
            int a = i * 2;
            int indices[6] = {a, a+1, a+2, a+2, a+1, a+3};
            if (c.curveYAxis) std::swap(indices[1],indices[2]), std::swap(indices[4],indices[5]);
            for (int k=0;k<6;++k) triangles[i*6+k]=indices[k];
        }
    }
    auto result = Mesh::New_ctor();
    result->set_vertices(vertices); result->set_uv(uv); result->set_triangles(triangles);
    result->RecalculateBounds(); result->RecalculateNormals();
    return result;
}
void CreateScreens() {
    auto shader = Shader::Find("Unlit/Texture");
    if (!shader) throw std::runtime_error("Unlit/Texture shader unavailable in Quest player");
    material = Material::New_ctor(shader);
    material->set_mainTexture(player->get_texture());
    float aspect = player->get_height() ? static_cast<float>(player->get_width()) / player->get_height() : 16.f/9.f;
    mesh = BuildMesh(aspect);
    for (size_t i=0;i<selected->screens.size();++i) {
        auto object = GameObject::New_ctor(i == 0 ? "CinemaScreen" : "TheaterAdditionalScreen");
        int layer = LayerMask::NameToLayer("Environment");
        if (layer >= 0) object->set_layer(layer);
        object->get_transform()->SetParent(root->get_transform(),false);
        auto& s = selected->screens[i];
        object->get_transform()->set_position(Vector3(s.position.x,s.position.y,s.position.z));
        object->get_transform()->set_rotation(Quaternion::Euler(s.rotation.x,s.rotation.y,s.rotation.z));
        object->get_transform()->set_localScale(Vector3(s.scale.x,s.scale.y,s.scale.z));
        object->AddComponent<MeshFilter*>()->set_sharedMesh(mesh);
        object->AddComponent<MeshRenderer*>()->set_sharedMaterial(material);
        object->SetActive(false); screens.push_back(object);
    }
    theater::StartScreenEffects(*selected,screens,mesh,material,aspect);
}
void Begin(AudioTimeSyncController* clock) {
    Cleanup();
    if (!selected || !getModConfig().Enabled.GetValue()) return;
    owner = clock;
    if (!theater::VideoAvailable(*selected)) return;
    root = GameObject::New_ctor("BeatSaberTheaterQuest");
    player = root->AddComponent<Video::VideoPlayer*>();
    player->set_playOnAwake(false);
    player->set_source(Video::VideoSource::Url);
    player->set_renderMode(Video::VideoRenderMode::APIOnly);
    player->set_audioOutputMode(Video::VideoAudioOutputMode::None);
    player->set_isLooping(selected->loop);
    player->set_waitForFirstFrame(true);
    player->set_skipOnDrop(true);
    player->set_url(selected->file.string());
    prepareStarted = Time::get_realtimeSinceStartup();
    player->Prepare();
    PaperLogger.info("Preparing local video: {}",selected->file.string());
}
void Tick(AudioTimeSyncController* clock) {
    if (clock != owner || !selected) return;
    if (!player) {
        if (!theater::VideoAvailable(*selected)) return;
        Begin(clock);
    }
    if (!getModConfig().Enabled.GetValue()) { Cleanup(); return; }
    float now = Time::get_realtimeSinceStartup();
    if (!player->get_isPrepared()) {
        float timeout = std::clamp(getModConfig().PrepareTimeout.GetValue(),5.f,120.f);
        if (now - prepareStarted > timeout) { PaperLogger.error("Video preparation timed out (missing/unsupported decoder?)"); Cleanup(); }
        return;
    }
    if (!clock->get_isReady()) return;
    auto audio = clock->_audioSource;
    if (!audio || !audio->get_isPlaying()) { player->Pause(); PausedScreenEffects(); return; }
    double songTime = audio->get_time();
    double duration = player->get_length();
    double raw = songTime * selected->speed + selected->offset;
    double target = theater::ReferenceTime(songTime,*selected,duration);
    if (raw < 0 || (selected->end >= 0 && target >= selected->end) || (!selected->loop && duration > 0 && raw >= duration)) {
        player->Pause(); Visibility(false);
        started = false;
        return;
    }
    if (screens.empty()) CreateScreens();
    if (paused) { player->Pause(); PausedScreenEffects(); return; }
    if (player->get_canSetPlaybackSpeed()) player->set_playbackSpeed(clock->get_timeScale() * selected->speed);
    float tolerance = std::clamp(getModConfig().SyncTolerance.GetValue(),.1f,2.f);
    // Rate limit seeks. Decoder time changes asynchronously, especially near loop boundaries.
    if (!started || (!seekPending && !clock->forcedNoAudioSync && std::abs(player->get_time() - target) > tolerance && now - lastSeek > 1.f)) {
        if (player->get_canSetTime()) { player->set_time(target); seekPending = true; lastSeek = now; }
        started = true;
    }
    if (seekPending && (std::abs(player->get_time() - target) < tolerance || now - lastSeek > 2.f)) seekPending = false;
    if (!player->get_isPlaying()) player->Play();
    // Avoid exposing the initial decoder frame before the first seek catches up.
    Visibility(!seekPending && player->get_texture() != nullptr);
    material->set_mainTexture(player->get_texture());
}
}
MAKE_HOOK_MATCH(TheaterInstall, &GlobalNamespace::GameplayCoreInstaller::InstallBindings, void, GlobalNamespace::GameplayCoreInstaller* self) {
    Cleanup(); selected.reset();
    TheaterInstall(self);
    try {
        auto data = self->_sceneSetupData;
        if (!data || !data->beatmapLevel) return;
        auto level = SongCore::API::Loading::GetLevelByLevelID(static_cast<std::string>(data->beatmapLevel->levelID));
        if (!level) return;
        auto levelID = static_cast<std::string>(data->beatmapLevel->levelID);
        selected = theater::LoadVideoConfig(std::filesystem::path(level->customLevelPath),levelID);
        if (selected && !theater::VideoAvailable(*selected)) theater::QueueMap(std::filesystem::path(level->customLevelPath),levelID);
        if (selected) for (auto& key : selected->unsupported) PaperLogger.warn("PC feature currently unsupported: {}",key);
    } catch (const std::exception& error) { PaperLogger.error("Config rejected: {}",error.what()); selected.reset(); }
}
MAKE_HOOK_MATCH(TheaterStart, &AudioTimeSyncController::Start, void, AudioTimeSyncController* self) {
    TheaterStart(self);
    try { Begin(self); } catch (const std::exception& error) { PaperLogger.error("Video startup failed: {}",error.what()); Cleanup(); }
}
MAKE_HOOK_MATCH(TheaterUpdate, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {
    TheaterUpdate(self);
    try { Tick(self); } catch (const std::exception& error) { PaperLogger.error("Video update failed: {}",error.what()); Cleanup(); }
}
MAKE_HOOK_MATCH(TheaterPause, &AudioTimeSyncController::Pause, void, AudioTimeSyncController* self) {
    TheaterPause(self); if (self == owner) { paused = true; if (player) player->Pause(); }
}
MAKE_HOOK_MATCH(TheaterResume, &AudioTimeSyncController::Resume, void, AudioTimeSyncController* self) {
    TheaterResume(self); if (self == owner) { paused = false; started = false; }
}
MAKE_HOOK_MATCH(TheaterDestroy, &AudioTimeSyncController::OnDestroy, void, AudioTimeSyncController* self) {
    if (self == owner) { Cleanup(); selected.reset(); }
    TheaterDestroy(self);
}
MAKE_HOOK_MATCH(TheaterVideoError, &Video::VideoPlayer::InvokeErrorReceivedCallback_Internal, void, Video::VideoPlayer* source, StringW error) {
    TheaterVideoError(source,error);
    if (source == player) { PaperLogger.error("Unity video decoder: {}",static_cast<std::string>(error)); Cleanup(); }
}
MAKE_HOOK_MATCH(TheaterSeekComplete, &Video::VideoPlayer::InvokeSeekCompletedCallback_Internal, void, Video::VideoPlayer* source) {
    TheaterSeekComplete(source);
    if (source == player) seekPending = false;
}
extern "C" __attribute__((visibility("default"))) void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
    Paper::Logger::RegisterFileContextId(PaperLogger.tag);
}
extern "C" __attribute__((visibility("default"))) void late_load() noexcept {
    il2cpp_functions::Init();
    getModConfig().Init(modInfo);
    getModConfig().AutoDownload.SetValue(false);
    if (getModConfig().HDDownloadsVersion.GetValue() < 1) {
        getModConfig().MaxVideoHeight.SetValue(1080);
        if(getModConfig().DownloadTimeout.GetValue()<900)getModConfig().DownloadTimeout.SetValue(900);
        getModConfig().HDDownloadsVersion.SetValue(1);
    }
    SongCore::API::Capabilities::RegisterCapability("Cinema");
    PaperLogger.info("Registered Cinema capability with SongCore");
    try { theater::InitDownloads(); }
    catch (const std::exception& error) { PaperLogger.error("Download initialization failed: {}",error.what()); }
    RegisterTheaterSettings();
    INSTALL_HOOK(PaperLogger,TheaterInstall);
    INSTALL_HOOK(PaperLogger,TheaterStart);
    INSTALL_HOOK(PaperLogger,TheaterUpdate);
    INSTALL_HOOK(PaperLogger,TheaterPause);
    INSTALL_HOOK(PaperLogger,TheaterResume);
    INSTALL_HOOK(PaperLogger,TheaterDestroy);
    INSTALL_HOOK(PaperLogger,TheaterVideoError);
    INSTALL_HOOK(PaperLogger,TheaterSeekComplete);
    PaperLogger.info("BeatSaberTheater Quest with manual Cinema video downloads loaded; target 1.40.8_7379");
}




