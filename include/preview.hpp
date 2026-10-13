#pragma once
#include <string>
namespace UnityEngine {class Texture;}
namespace UnityEngine::Video {class VideoPlayer;}
namespace theater {
void ToggleMenuPreview();
void StopMenuPreview();
void UpdateMenuPreview();
void MenuPreviewError(UnityEngine::Video::VideoPlayer* player,const std::string& error);
bool MenuPreviewActive();
std::string MenuPreviewStatus();
UnityEngine::Texture* MenuPreviewTexture();
}
