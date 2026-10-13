#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
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
    double offset{}, speed{1}, end{-1};
    bool loop{};
    float height{25}, curvature{};
    int segments{32};
    bool curveYAxis{};
    float bloom{1};
    bool disableDefaultModifications{};
    struct EnvironmentChange {
        std::string name, parentName, cloneFrom;
        std::optional<bool> active;
        std::optional<Vec3> position, rotation, scale;
    };
    std::vector<EnvironmentChange> environment;
    std::vector<Screen> screens{Screen{}};
    std::vector<std::string> unsupported;
};
std::optional<VideoConfig> LoadVideoConfig(const std::filesystem::path& directory, const std::string& levelID = {});
bool VideoAvailable(const VideoConfig& config);
double ReferenceTime(double songTime, const VideoConfig& config, double duration);
}
