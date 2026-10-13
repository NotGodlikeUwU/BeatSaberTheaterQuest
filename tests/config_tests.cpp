#include "video_config.hpp"
#include "bundled_configs.hpp"
#include "beatsaber-hook/shared/rapidjson/include/rapidjson/document.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
namespace fs = std::filesystem;
int checks = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void Write(const fs::path& folder, const char* json, const char* filename = "theater-video.json") {
    std::ofstream(folder / filename) << json;
}
void Reject(const fs::path& folder, const char* json) {
    Write(folder,json);
    bool rejected = false;
    try { theater::LoadVideoConfig(folder); } catch (const std::exception&) { rejected = true; }
    Check(rejected,"Invalid input accepted");
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Pass test scratch directory and Cinema fixture");
        fs::path folder = argv[1]; fs::create_directories(folder);
        Check(!theater::LoadVideoConfig(folder),"Missing config should disable playback");
        std::ofstream(folder / "clip.mp4") << "test fixture";
        Write(folder,R"({"videoFile":"clip.mp4","offset":-2000,"playbackSpeed":0.5,"loop":true,"screenPosition":{"x":2,"y":3,"z":20},"additionalScreens":[{"position":{"x":-2,"y":3,"z":20}}]})");
        auto c = *theater::LoadVideoConfig(folder);
        Check(c.screens.size()==2,"Additional screens lost");
        Check(c.screens[0].position.z==20,"Placement lost");
        Check(theater::ReferenceTime(0,c,10)==-2,"Negative offset must delay video");
        Check(theater::ReferenceTime(4,c,10)==0,"Offset/rate start boundary");
        Check(theater::ReferenceTime(30,c,10)==3,"Loop/reference speed mismatch");
        c.loop=false;
        Check(theater::ReferenceTime(30,c,10)==13,"Non-looping time must not wrap");
        Reject(folder,"[]"); Reject(folder,"{broken}");
        Reject(folder,R"({"videoFile":"../clip.mp4"})");
        Reject(folder,R"({"videoFile":"C:\\clip.mp4"})");
        Reject(folder,R"({"videoFile":"clip.mp4","playbackSpeed":0})");
        Reject(folder,R"({"videoFile":"clip.mp4","offset":"100"})");
        Reject(folder,R"({"videoFile":"clip.mp4","screenHeight":-5})");
        Reject(folder,R"({"videoFile":"clip.mp4","screenSubsurfaces":1000000})");
        Reject(folder,R"({"videoFile":"clip.mp4","screenPosition":{"z":1e100}})");
        Reject(folder,R"({"videoFile":"clip.mp4","loop":"true"})");
        Reject(folder,R"({"videoFile":"absent.mp4"})");
        Write(folder,R"({"downloadedFormats":{"Mp4":"C:\\maps\\clip.mp4"},"bloom":1})");
        c=*theater::LoadVideoConfig(folder);
        Check(c.file.filename()=="clip.mp4","PC download basename was not migrated");
        Check(c.unsupported.empty() && c.bloom==1,"PC bloom must be recognized");
        Write(folder,R"({"videoFile":"clip.mp4","bloom":0.5,"disableDefaultModifications":true,"environment":[{"name":"Construction","active":false},{"name":"Stage","parentName":"Environment","position":{"x":1,"y":2,"z":3},"scale":{"x":2,"y":2,"z":2}}]})");
        c=*theater::LoadVideoConfig(folder);
        Check(c.bloom==.5f && c.disableDefaultModifications,"PC effect settings lost");
        Check(c.environment.size()==2 && c.environment[0].active==false,"Environment visibility lost");
        Check(c.environment[1].position->z==3 && c.environment[1].parentName=="Environment","Environment placement lost");
        Reject(folder,R"({"videoFile":"clip.mp4","environment":[{"active":false}]})");
        Reject(folder,R"({"videoFile":"clip.mp4","bloom":"bright"})");
        fs::remove(folder/"theater-video.json");
        Write(folder,R"({"videoFile":"clip.mp4","offset":1250})","cinema-video.json");
        c=*theater::LoadVideoConfig(folder);
        Check(c.offset==1.25,"Legacy Cinema config not loaded");
        fs::remove(folder/"cinema-video.json");
        Write(folder,R"({"videoID":"krdQLzzAeZU","offset":-1200})");
        c=*theater::LoadVideoConfig(folder);
        Check(c.sourceUrl=="https://www.youtube.com/watch?v=krdQLzzAeZU","YouTube videoID not mapped");
        Check(c.file.extension()==".mp4","Download cache extension wrong");
        Check(!theater::VideoAvailable(c),"Missing remote video should await download");
        auto firstFile=c.file;
        c=*theater::LoadVideoConfig(folder);
        Check(c.file==firstFile,"Download cache name is not stable");
        Write(folder,R"({"videoID":"9bZkp7q19f0"})");
        c=*theater::LoadVideoConfig(folder);
        Check(c.file!=firstFile,"Different video sources share a cache name");
        Write(folder,R"({"videoUrl":"https://example.org/clip.mp4","videoFile":"download.mp4"})");
        c=*theater::LoadVideoConfig(folder);
        Check(c.file.filename()=="download.mp4","Explicit download destination ignored");
        Reject(folder,R"({"videoID":"bad/id"})");
        Reject(folder,R"({"videoUrl":"file:///etc/passwd"})");
        fs::remove(folder/"theater-video.json");
        c=*theater::LoadVideoConfig(folder,"custom_level_51CB5A9D50F73FEAC9380570CEC13A91D3824E64");
        Check(c.sourceUrl=="https://www.youtube.com/watch?v=D9G1VOjN_84","Bundled map binding missing");
        Check(c.offset==31.34,"Bundled offset lost");
        auto lower=*theater::LoadVideoConfig(folder,"custom_level_51cb5a9d50f73feac9380570cec13a91d3824e64 WIP");
        Check(lower.file==c.file,"Hash case or WIP suffix broke bundled binding");
        Write(folder,R"({"videoID":"krdQLzzAeZU","offset":100})");
        c=*theater::LoadVideoConfig(folder,"custom_level_51CB5A9D50F73FEAC9380570CEC13A91D3824E64");
        Check(c.offset==.1,"Mapper config must override bundled binding");
        fs::remove(folder/"theater-video.json");
        c=*theater::LoadVideoConfig(folder,"custom_level_7AA8BD16A692B8C8769D96CF96B9B259BEA2E862");
        Check(c.sourceUrl=="https://www.youtube.com/watch?v=ULjbIMvP6A0","Screenshot map's built-in Cinema video missing");
        Check(c.offset==-2,"Screenshot map's Cinema offset missing");
        Check(!c.title.empty(),"Screenshot map's video title missing");
        fs::copy_file(argv[2],folder/"cinema-video.json");
        c=*theater::LoadVideoConfig(folder);
        Check(c.sourceUrl=="https://www.youtube.com/watch?v=VDGG9zi53rQ","Real BeatSaver Cinema config videoID missing");
        Check(c.offset==-.66,"Real BeatSaver Cinema config offset missing");
        Check(c.file.filename().string().find(".mp4")!=std::string::npos,"Real BeatSaver Cinema videoFile missing");
        fs::remove(folder/"cinema-video.json");
        Write(folder,R"({"videoFile":"clip.mp4","colorCorrection":{"brightness":1.2,"contrast":9,"saturation":0,"hue":120,"exposure":0.5,"gamma":2},"vignette":{"type":"ellipse","radius":0.8,"softness":0.1},"transparency":true,"colorBlending":false,"environmentName":"OriginsEnvironment"})");
        c=*theater::LoadVideoConfig(folder);
        Check(c.unsupported.empty(),"Newly ported PC effects marked unsupported");
        Check(c.hasColorCorrection&&std::abs(c.color.brightness-1.2f)<.001f&&c.color.contrast==5,"PC correction values/clamping lost");
        Check(c.color.hue==120&&c.color.saturation==0&&c.color.gamma==2&&c.color.exposure==.5f,"Correction channels lost");
        Check(c.hasVignette&&c.vignette.elliptical&&std::abs(c.vignette.radius-.8f)<.001f,"Vignette shape lost");
        Check(c.transparency==true&&c.colorBlending==false&&c.environmentName=="OriginsEnvironment","Screen/environment flags lost");
        Write(folder,R"({"videoFile":"clip.mp4","configByMapper":true,"userSettings":{"customOffset":false}})");
        auto mapper=*theater::LoadVideoConfig(folder);
        Check(mapper.configByMapper&&!mapper.customOffset,"Mapper offset must remain locked until customized");
        theater::SaveVideoOffset(folder,"test",20);
        mapper=*theater::LoadVideoConfig(folder);
        Check(mapper.configByMapper&&mapper.customOffset,"Customize offset must persist and preserve mapper ownership");
        theater::SetVideoCustomOffset(folder,"test",false);
        mapper=*theater::LoadVideoConfig(folder);
        Check(!mapper.customOffset&&mapper.offset==0,"Disabling customization must restore a zero mapper offset");
        Write(folder,R"({"videoFile":"clip.mp4","configByMapper":true,"offset":-660.25,"title":"Mapper video","userSettings":{"otherSetting":true},"environmentName":"OriginsEnvironment"})");
        for(int cycle=0;cycle<3;cycle++) {
            theater::SetVideoCustomOffset(folder,"test",true);
            mapper=*theater::LoadVideoConfig(folder);
            Check(mapper.customOffset&&mapper.offset==-.66025,"Enabling customization changed mapper timing");
            theater::SaveVideoOffset(folder,"test",100+cycle);
            theater::SaveVideoOffset(folder,"test",200+cycle);
            theater::SetVideoCustomOffset(folder,"test",false);
            mapper=*theater::LoadVideoConfig(folder);
            Check(!mapper.customOffset&&mapper.offset==-.66025,"Repeated toggle cycle lost the original fractional negative offset");
            Check(mapper.configByMapper&&mapper.title=="Mapper video"&&mapper.environmentName=="OriginsEnvironment","Toggle discarded mapper metadata");
        }
        Write(folder,R"({"videoFile":"clip.mp4","configByMapper":true,"offset":900,"userSettings":{"customOffset":true,"originalOffset":1234.5}})");
        theater::SetVideoCustomOffset(folder,"test",false);
        mapper=*theater::LoadVideoConfig(folder);
        Check(!mapper.customOffset&&mapper.offset==1.2345,"Previously customized maps must restore the saved mapper offset");
        Write(folder,R"({"videoFile":"clip.mp4","colorCorrection":{"brightness":1.2},"environmentName":"OriginsEnvironment"})");
        theater::SaveVideoOffset(folder,"test",1234);
        c=*theater::LoadVideoConfig(folder);
        Check(c.offset==1.234&&c.environmentName=="OriginsEnvironment"&&c.hasColorCorrection,"Offset save discarded mapper configuration");
        theater::SaveVideoOffset(folder,"test",1254);
        c=*theater::LoadVideoConfig(folder);Check(c.offset==1.254,"Offset change not persistent");
        Reject(folder,R"({"videoFile":"clip.mp4","colorCorrection":{"hue":"invalid"}})");
        Reject(folder,R"({"videoFile":"clip.mp4","vignette":[]})");
        Reject(folder,R"({"videoFile":"clip.mp4","colorBlending":1})");
        fs::remove(folder/"theater-video.json");
        auto playlist=folder/"fixture.bplist";
        std::ofstream(playlist)<<R"({"songs":[{"hash":"abcdef","customData":{"cinema":{"videoID":"krdQLzzAeZU","offset":-20}}},{"levelid":"official-song","customData":{"theater":{"videoID":"9bZkp7q19f0","offset":100}}}]})";
        auto customBinding=theater::ReadPlaylistVideoConfig(playlist,"custom_level_ABCDEF");
        Check(!customBinding.empty(),"Playlist Cinema hash binding lost");
        c=*theater::LoadVideoConfig(folder,"custom_level_ABCDEF",customBinding);
        Check(c.offset==-.02&&c.sourceUrl.find("krdQLzzAeZU")!=std::string::npos,"Playlist config not loaded");
        auto officialBinding=theater::ReadPlaylistVideoConfig(playlist,"official-song");
        Check(!officialBinding.empty(),"Playlist official level binding lost");
        Check(theater::ReadPlaylistVideoConfig(playlist,"unrelated").empty(),"Playlist config applied to another song");
        theater::SaveVideoOffset(folder,"custom_level_ABCDEF",500,customBinding);
        c=*theater::LoadVideoConfig(folder);Check(c.offset==.5,"Playlist offset override did not materialize locally");
        theater::SetVideoCustomOffset(folder,"custom_level_ABCDEF",false,customBinding);
        c=*theater::LoadVideoConfig(folder);Check(!c.customOffset&&c.offset==-.02,"Disabling playlist offset must restore the playlist author's timing");
        Write(folder,R"({"videoID":"9bZkp7q19f0","offset":700})");
        c=*theater::LoadVideoConfig(folder,"custom_level_ABCDEF",customBinding);
        Check(c.offset==.7,"Map config must override playlist config");
        std::ofstream(folder/"theater-video.disabled")<<"disabled";
        Check(!theater::LoadVideoConfig(folder,"custom_level_ABCDEF",customBinding),"Deleted binding reappeared from playlist");
        theater::SaveVideoSource(folder,"https://www.youtube.com/watch?v=krdQLzzAeZU","Selected title","Selected author",123);
        c=*theater::LoadVideoConfig(folder);
        Check(c.title=="Selected title"&&c.author=="Selected author"&&c.duration==123&&c.offset==0,"Search selection not saved correctly");
        Check(!fs::exists(folder/"theater-video.disabled"),"New search selection did not re-enable the binding");
        fs::remove(folder/"theater-video.json");
        rapidjson::Document bindings; bindings.Parse(TheaterBundledConfigs);
        for (const auto& entry : bindings.GetArray()) {
            auto binding = theater::LoadVideoConfig(folder,entry["levelID"].GetString());
            Check(binding.has_value() && !binding->sourceUrl.empty(),"Bundled video source rejected");
        }
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
