#include "download_status_view.hpp"
#include "downloads.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include <algorithm>
DEFINE_TYPE(Theater, DownloadStatusView);
void Theater::DownloadStatusView::ctor() {
    INVOKE_CTOR();
    UnityEngine::MonoBehaviour::_ctor();
    label = nullptr; downloadButton = nullptr; nextRefresh = 0;
}
void Theater::DownloadStatusView::Update() {
    float now = UnityEngine::Time::get_realtimeSinceStartup();
    if (now < nextRefresh || !label || !UnityEngine::Object::op_Implicit_bool(label)) return;
    nextRefresh = now + .5f;
    try {
        label->set_text(theater::DownloadStatus());
        if (downloadButton && UnityEngine::Object::op_Implicit_bool(downloadButton)) {
            auto state = theater::SelectedDownload();
            downloadButton->set_interactable(state.configured && !state.ready && !state.busy);
        }
    }
    catch (const std::exception& error) { label->set_text(std::string("Downloader unavailable: ") + error.what()); }
    auto layout = label->get_gameObject()->GetComponent<UnityEngine::UI::LayoutElement*>();
    if (layout) layout->set_preferredHeight(std::max(24.f,label->get_preferredHeight() + 3.f));
}
