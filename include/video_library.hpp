#pragma once
#include "video_config.hpp"
namespace theater {
std::filesystem::path VideoDirectory(const std::string& levelID);
std::string PlaylistBinding(const std::string& levelID,const std::string& packID);
}
