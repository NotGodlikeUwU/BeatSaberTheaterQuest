#pragma once
#include <filesystem>
#include <string>
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
}
