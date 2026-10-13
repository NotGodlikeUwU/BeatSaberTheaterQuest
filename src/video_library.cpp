#include "video_library.hpp"
#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "songcore/shared/SongCore.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
namespace theater {
std::filesystem::path VideoDirectory(const std::string& levelID) {
    if(levelID.empty())return {};
    if(auto level=SongCore::API::Loading::GetLevelByLevelID(levelID))return std::filesystem::path(level->customLevelPath);
    // Official levels use mod-owned storage; this never writes to DLC or APK assets.
    auto normalized=NormalizeLevelID(levelID);
    std::string name=normalized;
    for(char& c:name)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='_'&&c!='-')c='_';
    if(name.size()>96)name.resize(96);
    uint64_t hash=14695981039346656037ULL;for(unsigned char c:normalized){hash^=c;hash*=1099511628211ULL;}
    std::ostringstream suffix;suffix<<'-'<<std::hex<<hash;
    return std::filesystem::path(getDataDir(std::string_view("bstheater")))/"TheaterOSTVideos"/(name+suffix.str());
}
std::string PlaylistBinding(const std::string& levelID,const std::string& packID) {
    constexpr std::string_view prefix="custom_levelPack_";
    if(!packID.starts_with(prefix))return {};
    auto directory=std::filesystem::weakly_canonical(std::filesystem::path(getDataDir(std::string_view("PlaylistManager")))/"Playlists");
    auto path=std::filesystem::weakly_canonical(std::filesystem::path(packID.substr(prefix.size())));
    auto relative=path.lexically_relative(directory);
    if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")return {};
    return ReadPlaylistVideoConfig(path,levelID);
}
}
