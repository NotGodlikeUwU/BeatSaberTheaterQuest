#include "video_config.hpp"
#include "bundled_configs.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/writer.h"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/stringbuffer.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <regex>
#include <iomanip>
#include <algorithm>
namespace theater {
namespace {
double Number(const rapidjson::Value& doc, const char* name, double fallback) {
    if (!doc.HasMember(name) || doc[name].IsNull()) return fallback;
    if (!doc[name].IsNumber() || !std::isfinite(doc[name].GetDouble())) throw std::runtime_error(std::string("Invalid number: ") + name);
    return doc[name].GetDouble();
}
bool Boolean(const rapidjson::Value& doc, const char* name, bool fallback) {
    if (!doc.HasMember(name) || doc[name].IsNull()) return fallback;
    if (!doc[name].IsBool()) throw std::runtime_error(std::string("Invalid boolean: ") + name);
    return doc[name].GetBool();
}
Vec3 Vector(const rapidjson::Value& doc, const char* name, Vec3 fallback) {
    if (!doc.HasMember(name) || doc[name].IsNull()) return fallback;
    auto& v = doc[name];
    if (!v.IsObject()) throw std::runtime_error(std::string("Invalid vector: ") + name);
    double x = Number(v,"x",fallback.x), y = Number(v,"y",fallback.y), z = Number(v,"z",fallback.z);
    if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) throw std::runtime_error("Vector exceeds supported bounds");
    return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
}
std::filesystem::path Media(const std::filesystem::path& directory, const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos || name.find(':') != std::string::npos || name == "." || name == "..") throw std::runtime_error("videoFile must be a filename inside the map folder");
    auto root = std::filesystem::weakly_canonical(directory);
    auto file = std::filesystem::weakly_canonical(root / name);
    if (file.parent_path() != root) throw std::runtime_error("Media escapes map folder");
    return file;
}
}
std::string NormalizeLevelID(std::string id) {
    std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (id.starts_with("CUSTOM_LEVEL_") && id.size() > 53) id.resize(53);
    return id;
}
namespace {
rapidjson::Document ConfigDocument(const std::filesystem::path& directory, const std::string& levelID, std::string_view playlistConfig) {
    if(std::filesystem::exists(directory/"theater-video.disabled"))return rapidjson::Document{};
    auto path = directory / "theater-video.json";
    if (!std::filesystem::exists(path)) path = directory / "cinema-video.json";
    rapidjson::Document doc;
    if (std::filesystem::exists(path)) {
        if (std::filesystem::file_size(path) > 1024 * 1024) throw std::runtime_error("Video config exceeds 1 MiB");
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot read video config");
        std::stringstream text; text << input.rdbuf();
        doc.Parse(text.str().c_str());
    } else if (!playlistConfig.empty()) {
        if (playlistConfig.size() > 1024*1024) throw std::runtime_error("Playlist video config exceeds 1 MiB");
        doc.Parse(playlistConfig.data(),playlistConfig.size());
    } else {
        if (levelID.empty()) return doc;
        static const rapidjson::Document bundled = [] { rapidjson::Document d; d.Parse(TheaterBundledConfigs); return d; }();
        const auto normalized = NormalizeLevelID(levelID);
        for (const auto& entry : bundled.GetArray()) {
            if (NormalizeLevelID(entry["levelID"].GetString()) == normalized) { doc.CopyFrom(entry["config"],doc.GetAllocator()); break; }
        }
    }
    return doc;
}
void WriteDocument(const std::filesystem::path& directory, const rapidjson::Document& doc) {
    std::filesystem::create_directories(directory);
    auto path=directory/"theater-video.json", partial=directory/"theater-video.json.part";
    rapidjson::StringBuffer text; rapidjson::Writer<rapidjson::StringBuffer> writer(text);doc.Accept(writer);
    {std::ofstream output(partial,std::ios::binary|std::ios::trunc);output.write(text.GetString(),text.GetSize());output.flush();if(!output)throw std::runtime_error("Cannot save video configuration");}
    std::error_code error;std::filesystem::rename(partial,path,error);
#ifdef _WIN32
    if(error){std::filesystem::remove(path);std::filesystem::rename(partial,path);}
#else
    if(error)throw std::runtime_error("Cannot publish video configuration: "+error.message());
#endif
}
void StringMember(rapidjson::Document& doc,const char* name,const std::string& value) {
    doc.RemoveMember(name);auto& a=doc.GetAllocator();doc.AddMember(rapidjson::Value(name,a),rapidjson::Value(value.c_str(),a),a);
}
}
std::optional<VideoConfig> LoadVideoConfig(const std::filesystem::path& directory, const std::string& levelID, std::string_view playlistConfig) {
    auto doc=ConfigDocument(directory,levelID,playlistConfig);
    if(doc.HasParseError())throw std::runtime_error("Invalid video JSON");
    if(doc.IsNull())return std::nullopt;
    if (doc.HasParseError() || !doc.IsObject()) throw std::runtime_error("Invalid video JSON");
    VideoConfig config;
    if (doc.HasMember("title") && doc["title"].IsString()) config.title = doc["title"].GetString();
    if (doc.HasMember("author") && doc["author"].IsString()) config.author = doc["author"].GetString();
    config.duration=static_cast<int>(std::clamp(Number(doc,"duration",0),0.0,86400.0));
    if(doc.HasMember("environmentName") && !doc["environmentName"].IsNull()) {
        if(!doc["environmentName"].IsString() || doc["environmentName"].GetStringLength()>128)throw std::runtime_error("Invalid environmentName");
        config.environmentName=doc["environmentName"].GetString();
    }
    for(auto key:{"transparency","colorBlending"})if(doc.HasMember(key)&&!doc[key].IsNull())
        (std::string(key)=="transparency"?config.transparency:config.colorBlending)=Boolean(doc,key,false);
    if(doc.HasMember("colorCorrection")&&!doc["colorCorrection"].IsNull()) {
        const auto& c=doc["colorCorrection"];if(!c.IsObject())throw std::runtime_error("Invalid colorCorrection");
        config.hasColorCorrection=true;
        config.color.brightness=std::clamp(static_cast<float>(Number(c,"brightness",1)),0.f,2.f);
        config.color.contrast=std::clamp(static_cast<float>(Number(c,"contrast",1)),0.f,5.f);
        config.color.saturation=std::clamp(static_cast<float>(Number(c,"saturation",1)),0.f,5.f);
        config.color.hue=std::clamp(static_cast<float>(Number(c,"hue",0)),-360.f,360.f);
        config.color.exposure=std::clamp(static_cast<float>(Number(c,"exposure",1)),0.f,5.f);
        config.color.gamma=std::clamp(static_cast<float>(Number(c,"gamma",1)),0.f,5.f);
    }
    if(doc.HasMember("vignette")&&!doc["vignette"].IsNull()) {
        const auto& v=doc["vignette"];if(!v.IsObject())throw std::runtime_error("Invalid vignette");
        config.hasVignette=true;
        config.vignette.radius=std::clamp(static_cast<float>(Number(v,"radius",1)),0.f,1.f);
        config.vignette.softness=std::clamp(static_cast<float>(Number(v,"softness",.005)),0.f,1.f);
        if(v.HasMember("type")&&!v["type"].IsNull()) {
            if(!v["type"].IsString())throw std::runtime_error("Invalid vignette type");
            std::string type=v["type"].GetString();config.vignette.elliptical=type=="oval"||type=="elliptical"||type=="ellipse";
        }
    }
    if (doc.HasMember("videoUrl") && doc["videoUrl"].IsString() && doc["videoUrl"].GetStringLength()) {
        config.sourceUrl = doc["videoUrl"].GetString();
        if (!config.sourceUrl.starts_with("https://") || config.sourceUrl.size() > 8192) throw std::runtime_error("videoUrl must use HTTPS");
    } else if (doc.HasMember("videoID") && doc["videoID"].IsString()) {
        std::string id = doc["videoID"].GetString();
        if (!std::regex_match(id,std::regex("[A-Za-z0-9_-]{11}"))) throw std::runtime_error("Invalid YouTube videoID");
        config.sourceUrl = "https://www.youtube.com/watch?v=" + id;
        if (config.title.empty()) config.title = id;
    }
    config.offset = Number(doc,"offset",0) / 1000.0;
    config.speed = Number(doc,"playbackSpeed",1);
    config.end = Number(doc,"endVideoAt",-1);
    config.loop = Boolean(doc,"loop",false);
    config.height = static_cast<float>(Number(doc,"screenHeight",25));
    config.curvature = static_cast<float>(Number(doc,"screenCurvature",0));
    double segments = Number(doc,"screenSubsurfaces",32);
    config.curveYAxis = Boolean(doc,"curveYAxis",false);
    config.bloom = static_cast<float>(Number(doc,"bloom",1));
    if (config.bloom < 0 || config.bloom > 2) throw std::runtime_error("Bloom must be between 0 and 2");
    config.disableDefaultModifications = Boolean(doc,"disableDefaultModifications",false);
    if (doc.HasMember("environment") && !doc["environment"].IsNull()) {
        auto& changes = doc["environment"];
        if (!changes.IsArray() || changes.Size() > 256) throw std::runtime_error("Too many environment modifications");
        for (auto& value : changes.GetArray()) {
            if (!value.IsObject() || !value.HasMember("name") || !value["name"].IsString()) throw std::runtime_error("Environment modification needs a name");
            VideoConfig::EnvironmentChange change;
            change.name = value["name"].GetString();
            for (auto key : {"parentName", "cloneFrom"}) if (value.HasMember(key) && value[key].IsString())
                (std::string(key) == "parentName" ? change.parentName : change.cloneFrom) = value[key].GetString();
            if (value.HasMember("active") && !value["active"].IsNull()) change.active = Boolean(value,"active",true);
            if (value.HasMember("position") && !value["position"].IsNull()) change.position = Vector(value,"position",{});
            if (value.HasMember("rotation") && !value["rotation"].IsNull()) change.rotation = Vector(value,"rotation",{});
            if (value.HasMember("scale") && !value["scale"].IsNull()) change.scale = Vector(value,"scale",{1,1,1});
            config.environment.push_back(std::move(change));
        }
    }
    if (config.speed < 0.1 || config.speed > 4 || config.height <= 0 || config.height > 500 || std::abs(config.curvature) > 360 || segments < 1 || segments > 128 || std::floor(segments) != segments) throw std::runtime_error("Screen or playback value outside supported bounds");
    config.segments = static_cast<int>(segments);
    config.screens[0].position = Vector(doc,"screenPosition",config.screens[0].position);
    config.screens[0].rotation = Vector(doc,"screenRotation",config.screens[0].rotation);
    if (doc.HasMember("additionalScreens") && !doc["additionalScreens"].IsNull()) {
        auto& screens = doc["additionalScreens"];
        if (!screens.IsArray() || screens.Size() > 7) throw std::runtime_error("At most seven additional screens supported");
        for (auto& value : screens.GetArray()) {
            if (!value.IsObject()) throw std::runtime_error("Invalid additional screen");
            Screen screen = config.screens[0];
            screen.position = Vector(value,"position",screen.position);
            screen.rotation = Vector(value,"rotation",screen.rotation);
            screen.scale = Vector(value,"scale",screen.scale);
            config.screens.push_back(screen);
        }
    }
    if (doc.HasMember("videoFile") && doc["videoFile"].IsString()) config.file = Media(directory,doc["videoFile"].GetString());
    // Reuse only the basename of absolute PC download paths.
    if ((config.file.empty() || !std::filesystem::is_regular_file(config.file)) && doc.HasMember("downloadedFormats") && doc["downloadedFormats"].IsObject()) {
        for (auto it = doc["downloadedFormats"].MemberBegin(); it != doc["downloadedFormats"].MemberEnd(); ++it) {
            if (!it->value.IsString()) continue;
            std::string name = it->value.GetString();
            auto slash = name.find_last_of("/\\");
            if (slash != std::string::npos) name.erase(0, slash + 1);
            auto candidate = Media(directory,name);
            if (candidate.extension() == ".mp4" && std::filesystem::is_regular_file(candidate)) { config.file = candidate; break; }
        }
    }
    if (config.file.empty() && !config.sourceUrl.empty()) {
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : config.sourceUrl) { hash ^= c; hash *= 1099511628211ULL; }
        std::ostringstream name; name << "bstheater-video-" << std::hex << hash << ".mp4";
        config.file = Media(directory,name.str());
    }
    if ((config.file.empty() || !std::filesystem::is_regular_file(config.file)) && config.sourceUrl.empty()) throw std::runtime_error("Video config has neither local media nor a download source");
    if (config.file.extension() != ".mp4") throw std::runtime_error("Quest port currently requires MP4");
    return config;
}
void SaveVideoSource(const std::filesystem::path& directory,const std::string& source,const std::string& title,const std::string& author,int duration) {
    if(!source.starts_with("https://")||source.size()>8192)throw std::runtime_error("Video source must use HTTPS");
    rapidjson::Document doc;doc.SetObject();
    doc.RemoveMember("videoID");doc.RemoveMember("videoFile");doc.RemoveMember("downloadedFormats");
    StringMember(doc,"videoUrl",source);StringMember(doc,"title",title);StringMember(doc,"author",author);
    doc.RemoveMember("duration");doc.AddMember("duration",std::clamp(duration,0,86400),doc.GetAllocator());
    WriteDocument(directory,doc);
    std::filesystem::remove(directory/"theater-video.disabled");
}
void SaveVideoOffset(const std::filesystem::path& directory,const std::string& levelID,int milliseconds,std::string_view playlistConfig) {
    auto doc=ConfigDocument(directory,levelID,playlistConfig);
    if(!doc.IsObject()||doc.HasParseError())throw std::runtime_error("No video configuration selected");
    if(std::abs(static_cast<int64_t>(milliseconds))>86400000)throw std::runtime_error("Offset exceeds one day");
    auto& a=doc.GetAllocator();
    if(!doc.HasMember("userSettings")||!doc["userSettings"].IsObject()){doc.RemoveMember("userSettings");doc.AddMember("userSettings",rapidjson::Value(rapidjson::kObjectType),a);}
    auto& settings=doc["userSettings"];
    if(!settings.HasMember("originalOffset"))settings.AddMember("originalOffset",Number(doc,"offset",0),a);
    settings.RemoveMember("customOffset");settings.AddMember("customOffset",true,a);
    doc.RemoveMember("offset");doc.AddMember("offset",milliseconds,a);WriteDocument(directory,doc);
}
std::string ReadPlaylistVideoConfig(const std::filesystem::path& playlist,const std::string& levelID) {
    if(!std::filesystem::is_regular_file(playlist))return {};
    if(std::filesystem::file_size(playlist)>16*1024*1024)throw std::runtime_error("Playlist exceeds 16 MiB");
    std::ifstream input(playlist);std::stringstream text;text<<input.rdbuf();
    rapidjson::Document doc;doc.Parse(text.str().c_str());
    if(doc.HasParseError()||!doc.IsObject()||!doc.HasMember("songs")||!doc["songs"].IsArray())throw std::runtime_error("Invalid playlist JSON");
    auto id=NormalizeLevelID(levelID);
    for(const auto& song:doc["songs"].GetArray()) {
        if(!song.IsObject())continue;
        bool matches=false;
        for(auto key:{"levelid","levelID","hash"})if(song.HasMember(key)&&song[key].IsString()) {
            std::string value=song[key].GetString();if(std::string(key)=="hash")value="custom_level_"+value;
            if(!value.empty()&&NormalizeLevelID(value)==id)matches=true;
        }
        if(!matches)continue;
        const rapidjson::Value* data=&song;
        if(song.HasMember("customData")&&song["customData"].IsObject())data=&song["customData"];
        for(auto key:{"theater","cinema"})if(data->HasMember(key)&&(*data)[key].IsObject()) {
            rapidjson::StringBuffer result;rapidjson::Writer<rapidjson::StringBuffer> writer(result);(*data)[key].Accept(writer);
            if(result.GetSize()>1024*1024)throw std::runtime_error("Playlist video config exceeds 1 MiB");
            return result.GetString();
        }
    }
    return {};
}
bool VideoAvailable(const VideoConfig& config) {
    std::error_code error;
    return std::filesystem::is_regular_file(config.file,error) && std::filesystem::file_size(config.file,error) > 0 && !error;
}
double ReferenceTime(double songTime, const VideoConfig& config, double duration) {
    double result = songTime * config.speed + config.offset;
    if (config.loop && duration > 0 && result >= 0) result = std::fmod(result, duration);
    return result;
}
}

