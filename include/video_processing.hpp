#pragma once
#include "video_config.hpp"
#include <memory>
namespace UnityEngine {class Material;class Texture;class GameObject;class Mesh;}
namespace theater {
UnityEngine::Material* CreateVideoMaterial(const VideoConfig& config,bool menu=false);
class VideoImageProcessor {
public:
    explicit VideoImageProcessor(const VideoConfig& config);
    ~VideoImageProcessor();
    UnityEngine::Texture* Update(UnityEngine::Texture* texture);
    void AttachScreenBody(UnityEngine::GameObject* screen,UnityEngine::Mesh* mesh);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
