#pragma once
#include <filesystem>
#include <string>
#include "video_config.hpp"
namespace GlobalNamespace { class BeatmapLevel; struct BeatmapKey; }
namespace theater {
void InitDownloads();
void SetDownloadOptions(bool automatic, int height, int timeoutSeconds, int maxMiB);
void QueueMap(const std::filesystem::path& directory, const std::string& levelID, bool manual = false);
void RetrySelectedDownload();
void DeleteSelectedVideo();
void QueueAllVideos();
void CancelDownloads();
std::string DownloadStatus();
struct SelectedDownloadState { bool configured{}, ready{}, busy{}; std::string text; };
SelectedDownloadState SelectedDownload();
struct SelectedVideoInfo {
    std::string levelID, query, playlistConfig;
    std::filesystem::path directory;
    std::optional<VideoConfig> config;
    bool playbackAllowed{true}, busy{};
};
SelectedVideoInfo SelectedVideo();
GlobalNamespace::BeatmapLevel* SelectedLevel();
std::optional<VideoConfig> LoadLevelVideo(const std::string& levelID);
bool PlaybackAllowed(const GlobalNamespace::BeatmapKey& key);
void ChangeSelectedOffset(int deltaMilliseconds);
void SetSelectedCustomOffset(bool enabled);
void DeleteSelectedConfig();
struct VideoSearchResult { std::string url,title,author,thumbnail; int duration{}; };
struct VideoSearchState {
    uint64_t revision{};
    bool busy{};
    std::string query, message, levelID;
    std::vector<VideoSearchResult> results;
};
void SearchVideos(const std::string& query);
VideoSearchState SearchResults();
void ChooseSearchVideo(int index);
}
