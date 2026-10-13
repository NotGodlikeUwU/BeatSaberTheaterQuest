#pragma once
#include "video_config.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Mesh.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Texture.hpp"
namespace theater {
void StartScreenEffects(const VideoConfig&, const std::vector<UnityEngine::GameObject*>&, UnityEngine::Mesh*, UnityEngine::Material*, float aspect);
void UpdateScreenLight(UnityEngine::Texture*, bool visible);
void StopScreenEffects();
}
