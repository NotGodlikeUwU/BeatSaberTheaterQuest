#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <string_view>
namespace theater {
struct Vec3 { float x{}, y{}, z{}; };
struct Screen {
    Vec3 position{0, 12.4f, 67.8f};
    Vec3 rotation{-7, 0, 0};
    Vec3 scale{1, 1, 1};
};
struct VideoConfig {
    std::filesystem::path file;
    std::string sourceUrl;
    std::string title;
    std::string author, environmentName;
    int duration{};
    double offset{}, speed{1}, end{-1};
    bool loop{};
    float height{25}, curvature{};
    int segments{32};
    bool curveYAxis{};
    float bloom{1};
    bool disableDefaultModifications{};
    // PC compatibility: transparency=true requests an opaque screen body.
    std::optional<bool> transparency, colorBlending;
    struct ColorCorrection {
        float brightness{1}, contrast{1}, saturation{1}, hue{}, exposure{1}, gamma{1};
    } color;
    struct Vignette { float radius{1}, softness{.005f}; bool elliptical{}; } vignette;
    bool hasColorCorrection{}, hasVignette{};
    struct EnvironmentChange {
        std::string name, parentName, cloneFrom;
        std::optional<bool> active;
        std::optional<Vec3> position, rotation, scale;
    };
    std::vector<EnvironmentChange> environment;
    std::vector<Screen> screens{Screen{}};
    std::vector<std::string> unsupported;
};
std::optional<VideoConfig> LoadVideoConfig(const std::filesystem::path& directory, const std::string& levelID = {}, std::string_view playlistConfig = {});
void SaveVideoSource(const std::filesystem::path& directory, const std::string& source, const std::string& title, const std::string& author, int duration);
void SaveVideoOffset(const std::filesystem::path& directory, const std::string& levelID, int milliseconds, std::string_view playlistConfig = {});
std::string NormalizeLevelID(std::string id);
std::string ReadPlaylistVideoConfig(const std::filesystem::path& playlist, const std::string& levelID);
bool VideoAvailable(const VideoConfig& config);
double ReferenceTime(double songTime, const VideoConfig& config, double duration);
}
