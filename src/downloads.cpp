#include "downloads.hpp"
#include "main.hpp"
#include "modconfig.hpp"
#include "video_config.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "songcore/shared/SongCore.hpp"
#include <jni.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <set>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <utf8.h>
#include <chrono>
#include <unordered_map>
#include <fstream>

extern "C" const unsigned char bstheater_download_dex_start[], bstheater_download_dex_end[];
extern "C" const unsigned char bstheater_desugar_dex_start[], bstheater_desugar_dex_end[];
extern "C" jstring Java_org_bstheater_quest_NativePlayerSolver_evaluate(JNIEnv*,jclass,jstring);
namespace theater {
namespace {
struct Task { std::filesystem::path directory; std::string levelID; bool manual; };
struct Service {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Task> queue;
    std::set<std::string> pending;
    std::unordered_map<std::string,std::chrono::steady_clock::time_point> retryAfter;
    std::string active, message{"Select a map and press Download Video"};
    std::string lastError;
    bool downloading{};
    uint64_t cancelEpoch{};
    std::filesystem::path selectedDir;
    std::string selectedID;
    std::optional<VideoConfig> selectedConfig;
    std::string selectedConfigError;
    unsigned completed{}, failed{};
    std::atomic<bool> automatic{false};
    std::atomic<bool> loginBlocked{false};
    std::atomic<int> height{1080}, timeout{300}, maxMiB{512};
    jclass bridge{};
    jmethodID download{}, cancel{}, prepare{};
    jfieldID bytes{}, total{}, phase{}, sessionStatus{}, loginRequired{};
};
Service& State() { static auto* service = new Service(); return *service; }
std::string JavaString(JNIEnv* env, jstring value) {
    if (!value) return {};
    const jchar* chars = env->GetStringChars(value,nullptr);
    if (!chars) return {};
    std::string result; utf8::utf16to8(chars,chars+env->GetStringLength(value),std::back_inserter(result));
    env->ReleaseStringChars(value,chars); return result;
}
jstring ToJavaString(JNIEnv* env, const std::string& value) {
    std::u16string utf16; utf8::utf8to16(value.begin(),value.end(),std::back_inserter(utf16));
    return env->NewString(reinterpret_cast<const jchar*>(utf16.data()),static_cast<jsize>(utf16.size()));
}
void Check(JNIEnv* env) {
    if (!env->ExceptionCheck()) return;
    auto exception = env->ExceptionOccurred(); env->ExceptionClear();
    auto klass = env->GetObjectClass(exception);
    auto toString = env->GetMethodID(klass,"toString","()Ljava/lang/String;");
    std::string message = "Android Java downloader error";
    if (toString) message = JavaString(env,static_cast<jstring>(env->CallObjectMethod(exception,toString)));
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(klass); env->DeleteLocalRef(exception);
    throw std::runtime_error(message);
}
JNIEnv* Environment() {
    if (!modloader_jvm) throw std::runtime_error("Scotland2 JVM unavailable");
    JNIEnv* env{};
    if (modloader_jvm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6) != JNI_OK && modloader_jvm->AttachCurrentThread(&env,nullptr) != JNI_OK)
        throw std::runtime_error("Cannot attach download worker to Android JVM");
    return env;
}
void Worker() {
    JNIEnv* env{};
    try { env = Environment(); }
    catch (const std::exception& e) { auto& state = State(); std::lock_guard guard(state.mutex); state.message = e.what(); return; }
    for (;;) {
        auto& state = State();
        Task task;
        uint64_t epoch;
        {
            std::unique_lock guard(state.mutex);
            state.wake.wait(guard,[&] { return !state.queue.empty(); });
            task = std::move(state.queue.front()); state.queue.pop_front();
            state.active = task.directory.filename().string();
            state.message = "Checking video config";
            state.downloading = false; epoch = state.cancelEpoch;
        }
        bool failed = false;
        try {
            if (task.manual || state.automatic.load()) {
                auto config = LoadVideoConfig(task.directory,task.levelID);
                bool upgrade=false;
                if(config && VideoAvailable(*config) && !config->sourceUrl.empty()) {
                    upgrade=task.manual;
                }
                if (config && (!VideoAvailable(*config) || upgrade)) {
                    if (config->sourceUrl.empty()) throw std::runtime_error("No videoID/videoUrl in map config");
                    {
                        std::lock_guard guard(state.mutex);
                        if (epoch != state.cancelEpoch) throw std::runtime_error("Download cancelled");
                        env->CallStaticVoidMethod(state.bridge,state.prepare); Check(env);
                        state.message = "Downloading video"; state.downloading = true;
                    }
                    PaperLogger.info("Downloading video for {}",task.levelID);
                    if (env->PushLocalFrame(32) != JNI_OK) { Check(env); throw std::runtime_error("Cannot allocate download JNI frame"); }
                    try {
                        auto source = ToJavaString(env,config->sourceUrl);
                        auto output = ToJavaString(env,config->file.string());
                        Check(env);
                        env->CallStaticVoidMethod(state.bridge,state.download,source,output,static_cast<jint>(state.height.load()),static_cast<jint>(state.timeout.load()),static_cast<jlong>(state.maxMiB.load()) * 1024 * 1024);
                        Check(env);
                        env->PopLocalFrame(nullptr);
                    } catch (...) { env->PopLocalFrame(nullptr); throw; }
                    if (!VideoAvailable(*config)) throw std::runtime_error("Download finished without a video file");
                    { std::lock_guard guard(state.mutex); ++state.completed; state.message = "Downloaded: " + state.active; state.lastError.clear(); state.loginBlocked = false; }
                    PaperLogger.info("Video ready: {}",config->file.string());
                } else if (task.manual) {
                    std::lock_guard guard(state.mutex);
                    state.message = config ? "Selected video already downloaded" : "Selected map has no Theater/Cinema video binding";
                }
            }
        } catch (const std::exception& error) {
            failed = true;
            std::lock_guard guard(state.mutex); ++state.failed;
            state.message = "Failed: " + state.active + " - " + error.what();
            if (state.bridge && env->GetStaticBooleanField(state.bridge,state.loginRequired)) {
                // All queued requests use the same session/IP. Stop retrying every map when login is rejected.
                ++state.cancelEpoch; state.queue.clear(); state.pending.clear();
                state.loginBlocked = true;
                state.message += "\nQueue stopped. Import youtube-cookies.txt and retry.";
            }
            state.lastError = state.message;
            state.retryAfter[task.directory.string()] = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            PaperLogger.error("Video download for {}: {}",task.levelID,error.what());
        }
        {
            std::lock_guard guard(state.mutex);
            state.pending.erase(task.directory.string()); state.active.clear(); state.downloading = false;
            if (!failed && state.queue.empty() && state.message == "Checking video config") state.message = "No missing configured videos";
        }
    }
}
void SongsLoaded(std::span<SongCore::SongLoader::CustomBeatmapLevel* const> levels) {
    for (auto* level : levels) {
        if (!level) continue;
        QueueMap(std::filesystem::path(level->customLevelPath),static_cast<std::string>(level->levelID));
    }
}
void LevelSelected(const SongCore::API::LevelSelect::LevelWasSelectedEventArgs& args) {
    auto* level = args.isCustom ? SongCore::API::Loading::GetLevelByLevelID(args.levelID) : nullptr;
    std::optional<VideoConfig> config;
    std::string configError;
    if (level) {
        try { config = LoadVideoConfig(std::filesystem::path(level->customLevelPath),args.levelID); }
        catch (const std::exception& error) { configError = error.what(); PaperLogger.error("Selected Cinema config rejected: {}",configError); }
    }
    auto& state = State();
    {
        std::lock_guard guard(state.mutex);
        state.selectedDir = level ? std::filesystem::path(level->customLevelPath) : std::filesystem::path{};
        state.selectedID = args.levelID;
        state.selectedConfig = std::move(config);
        state.selectedConfigError = std::move(configError);
    }
    if (level) QueueMap(std::filesystem::path(level->customLevelPath),args.levelID);
}
}
void InitDownloads() {
    auto& state = State(); auto* env = Environment();
    if (env->PushLocalFrame(64) != JNI_OK) { Check(env); throw std::runtime_error("Cannot allocate loader JNI frame"); }
    try {
        auto bufferClass = env->FindClass("java/nio/ByteBuffer"); Check(env);
        auto buffers = env->NewObjectArray(2,bufferClass,nullptr); Check(env);
        const unsigned char* starts[] = {bstheater_download_dex_start,bstheater_desugar_dex_start};
        const unsigned char* ends[] = {bstheater_download_dex_end,bstheater_desugar_dex_end};
        for (int i=0;i<2;++i) {
            auto buffer = env->NewDirectByteBuffer(const_cast<unsigned char*>(starts[i]),static_cast<jlong>(ends[i]-starts[i])); Check(env);
            env->SetObjectArrayElement(buffers,i,buffer); Check(env);
        }
        auto classLoader = env->FindClass("java/lang/ClassLoader"); Check(env);
        auto parentMethod = env->GetStaticMethodID(classLoader,"getSystemClassLoader","()Ljava/lang/ClassLoader;"); Check(env);
        auto parent = env->CallStaticObjectMethod(classLoader,parentMethod); Check(env);
        auto loaderClass = env->FindClass("dalvik/system/InMemoryDexClassLoader"); Check(env);
        auto constructor = env->GetMethodID(loaderClass,"<init>","([Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V"); Check(env);
        auto loader = env->NewObject(loaderClass,constructor,buffers,parent); Check(env);
        auto loadClass = env->GetMethodID(classLoader,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;"); Check(env);
        auto solverName = env->NewStringUTF("org.bstheater.quest.NativePlayerSolver"); Check(env);
        auto solver = static_cast<jclass>(env->CallObjectMethod(loader,loadClass,solverName)); Check(env);
        JNINativeMethod solverMethods[] = {{const_cast<char*>("evaluate"),const_cast<char*>("(Ljava/lang/String;)Ljava/lang/String;"),
            reinterpret_cast<void*>(Java_org_bstheater_quest_NativePlayerSolver_evaluate)}};
        if (env->RegisterNatives(solver,solverMethods,1) != JNI_OK) { Check(env); throw std::runtime_error("Cannot register player decoder"); }
        auto name = env->NewStringUTF("org.bstheater.quest.VideoDownloader"); Check(env);
        auto localBridge = static_cast<jclass>(env->CallObjectMethod(loader,loadClass,name)); Check(env);
        state.bridge = static_cast<jclass>(env->NewGlobalRef(localBridge)); Check(env);
        state.download = env->GetStaticMethodID(state.bridge,"download","(Ljava/lang/String;Ljava/lang/String;IIJ)V"); Check(env);
        state.cancel = env->GetStaticMethodID(state.bridge,"cancel","()V"); Check(env);
        state.prepare = env->GetStaticMethodID(state.bridge,"prepare","()V"); Check(env);
        state.bytes = env->GetStaticFieldID(state.bridge,"downloadedBytes","J"); Check(env);
        state.total = env->GetStaticFieldID(state.bridge,"totalBytes","J"); Check(env);
        state.phase = env->GetStaticFieldID(state.bridge,"phase","Ljava/lang/String;"); Check(env);
        state.sessionStatus = env->GetStaticFieldID(state.bridge,"sessionStatus","Ljava/lang/String;"); Check(env);
        state.loginRequired = env->GetStaticFieldID(state.bridge,"loginRequired","Z"); Check(env);
        auto cookieDirectory = std::filesystem::path(getDataDir(std::string_view("bstheater")));
        std::filesystem::create_directories(cookieDirectory);
        auto cookiePath = ToJavaString(env,(cookieDirectory / "youtube-cookies.txt").string()); Check(env);
        auto setCookieFile = env->GetStaticMethodID(state.bridge,"setCookieFile","(Ljava/lang/String;)V"); Check(env);
        env->CallStaticVoidMethod(state.bridge,setCookieFile,cookiePath); Check(env);
        env->PopLocalFrame(nullptr);
    } catch (const std::exception& error) {
        if (state.bridge) { env->DeleteGlobalRef(state.bridge); state.bridge = nullptr; }
        state.message = std::string("Downloader initialization failed: ") + error.what();
        env->PopLocalFrame(nullptr); throw;
    }
    SetDownloadOptions(getModConfig().AutoDownload.GetValue(),getModConfig().MaxVideoHeight.GetValue(),getModConfig().DownloadTimeout.GetValue(),getModConfig().MaxVideoMiB.GetValue());
    std::thread(Worker).detach();
    SongCore::API::Loading::GetSongsLoadedEvent().addCallback(SongsLoaded);
    SongCore::API::LevelSelect::GetLevelWasSelectedEvent().addCallback(LevelSelected);
    if (SongCore::API::Loading::AreSongsLoaded()) SongsLoaded(SongCore::API::Loading::GetAllLevels());
}
void SetDownloadOptions(bool automatic, int height, int timeout, int maxMiB) {
    auto& state = State(); state.automatic = false;
    state.height = std::clamp(height,144,1080); state.timeout = std::clamp(timeout,30,1800); state.maxMiB = std::clamp(maxMiB,32,2048);
}
void QueueMap(const std::filesystem::path& directory, const std::string& id, bool manual) {
    auto& state = State();
    if ((!manual && (!state.automatic.load() || state.loginBlocked.load())) || !state.bridge) return;
    std::lock_guard guard(state.mutex);
    auto retry = state.retryAfter.find(directory.string());
    if (!manual && retry != state.retryAfter.end() && retry->second > std::chrono::steady_clock::now()) return;
    if (!state.pending.insert(directory.string()).second) return;
    Task task{directory,id,manual};
    if (manual) state.queue.push_front(std::move(task)); else state.queue.push_back(std::move(task));
    state.wake.notify_one();
}
void RetrySelectedDownload() {
    auto& state = State(); std::filesystem::path directory; std::string id;
    {
        std::lock_guard guard(state.mutex);
        if ((state.selectedConfig && VideoAvailable(*state.selectedConfig)) || state.pending.contains(state.selectedDir.string())) return;
        directory = state.selectedDir; id = state.selectedID; state.lastError.clear();
    }
    if (directory.empty()) { std::lock_guard guard(state.mutex); state.message = "Select a custom beatmap first"; return; }
    QueueMap(directory,id,true);
}
void DeleteSelectedVideo() {
    auto& state=State();std::lock_guard guard(state.mutex);
    if(!state.selectedConfig || state.pending.contains(state.selectedDir.string()))return;
    try {
        auto file=std::filesystem::weakly_canonical(state.selectedConfig->file);
        if(file.parent_path()!=std::filesystem::weakly_canonical(state.selectedDir) || file.extension()!=".mp4")throw std::runtime_error("Video is outside the selected map folder");
        std::filesystem::remove(file);
        std::filesystem::remove(file.string()+".bstheater-quality");
        state.message="Selected video deleted";state.lastError.clear();state.retryAfter.erase(state.selectedDir.string());
        PaperLogger.info("Deleted selected Cinema video: {}",state.selectedID);
    }catch(const std::exception& error){state.message=std::string("Video deletion failed: ")+error.what();state.lastError=state.message;}
}
void QueueAllVideos() {
    for (auto* level : SongCore::API::Loading::GetAllLevels())
        if (level) QueueMap(std::filesystem::path(level->customLevelPath),static_cast<std::string>(level->levelID),true);
}
void CancelDownloads() {
    auto& state = State();
    { std::lock_guard guard(state.mutex); ++state.cancelEpoch; for (const auto& task : state.queue) state.pending.erase(task.directory.string()); state.queue.clear(); state.message = "Download queue cancelled"; }
    if (state.bridge) { auto* env = Environment(); env->CallStaticVoidMethod(state.bridge,state.cancel); Check(env); }
}
std::string DownloadStatus() {
    auto& state = State(); std::lock_guard guard(state.mutex);
    std::ostringstream text;
    if (state.bridge) {
        auto* env = Environment();
        auto session = static_cast<jstring>(env->GetStaticObjectField(state.bridge,state.sessionStatus)); Check(env);
        text << JavaString(env,session) << '\n'; env->DeleteLocalRef(session);
    }
    if (!state.selectedDir.empty()) {
        text << "Selected: " << state.selectedDir.filename().string() << '\n';
        if (state.selectedConfig) {
            if (!state.selectedConfig->title.empty()) text << state.selectedConfig->title << '\n';
            text << (VideoAvailable(*state.selectedConfig) ? "Video ready!" : "Video available - Download Video") << '\n';
        } else if (!state.selectedConfigError.empty()) text << "Cinema config error: " << state.selectedConfigError << '\n';
        else text << "No Cinema video configured for this map" << '\n';
    } else text << "Select a custom beatmap to see its Cinema video" << '\n';
    text << "Queued: " << state.queue.size() << " | Downloaded: " << state.completed << " | Failed: " << state.failed << '\n';
    if (state.active.empty()) text << state.message;
    else if (state.bridge && state.downloading) {
        auto* env = Environment();
        auto phase = static_cast<jstring>(env->GetStaticObjectField(state.bridge,state.phase)); Check(env);
        auto bytes = env->GetStaticLongField(state.bridge,state.bytes), total = env->GetStaticLongField(state.bridge,state.total); Check(env);
        text << state.active << "\n" << JavaString(env,phase); env->DeleteLocalRef(phase);
        if (bytes > 0) { text << " " << std::fixed << std::setprecision(1) << bytes / 1048576.0 << " MiB"; if (total > 0) text << " / " << total / 1048576.0 << " MiB"; }
    }
    else text << state.active << "\n" << state.message;
    if (!state.lastError.empty() && state.message != state.lastError) text << "\nLast error: " << state.lastError;
    return text.str();
}
SelectedDownloadState SelectedDownload() {
    auto& state=State();std::lock_guard guard(state.mutex);
    SelectedDownloadState result;
    if (!state.selectedConfig) return result;
    result.configured=true;result.ready=VideoAvailable(*state.selectedConfig);
    result.busy=state.pending.contains(state.selectedDir.string());
    result.text=result.ready?"Video ready!":"Video available";
    if (result.busy) {
        result.text="Queued";
        if(state.downloading && state.active==state.selectedDir.filename().string() && state.bridge) {
            auto* env=Environment();auto bytes=env->GetStaticLongField(state.bridge,state.bytes),total=env->GetStaticLongField(state.bridge,state.total);Check(env);
            result.text=total>0?"Downloading: "+std::to_string(bytes*100/total)+"%":"Resolving video";
        }
    }
    return result;
}
}
