#include "video_config.hpp"
#include "bundled_configs.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
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
std::optional<VideoConfig> LoadVideoConfig(const std::filesystem::path& directory, const std::string& levelID) {
    auto path = directory / "theater-video.json";
    if (!std::filesystem::exists(path)) path = directory / "cinema-video.json";
    rapidjson::Document doc;
    if (std::filesystem::exists(path)) {
        if (std::filesystem::file_size(path) > 1024 * 1024) throw std::runtime_error("Video config exceeds 1 MiB");
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot read video config");
        std::stringstream text; text << input.rdbuf();
        doc.Parse(text.str().c_str());
    } else {
        if (levelID.empty()) return std::nullopt;
        static const rapidjson::Document bundled = [] { rapidjson::Document d; d.Parse(TheaterBundledConfigs); return d; }();
        auto normalize = [](std::string id) {
            std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            if (id.starts_with("CUSTOM_LEVEL_") && id.size() > 53) id.resize(53);
            return id;
        };
        const auto normalized = normalize(levelID);
        for (const auto& entry : bundled.GetArray()) {
            if (normalize(entry["levelID"].GetString()) == normalized) { doc.CopyFrom(entry["config"],doc.GetAllocator()); break; }
        }
        if (doc.IsNull()) return std::nullopt;
    }
    if (doc.HasParseError() || !doc.IsObject()) throw std::runtime_error("Invalid video JSON");
    VideoConfig config;
    if (doc.HasMember("title") && doc["title"].IsString()) config.title = doc["title"].GetString();
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
    for (const auto* key : {"colorCorrection", "vignette", "transparency", "colorBlending", "environmentName"}) if (doc.HasMember(key) && !doc[key].IsNull()) config.unsupported.emplace_back(key);
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

