#pragma once

#include <imgui.h>

namespace ESPExplorerAE::ModalUtils
{
    // Keep this scope alive through BeginPopupModal/EndPopup: ImGui invokes the
    // size callback after construction and borrows the aspect-ratio value.
    class PopupSizing
    {
    public:
        PopupSizing(const ImVec2& initialSize, const ImVec2& minSize, const ImVec2& maxSize, bool keepAspectRatio = true);
        PopupSizing(const PopupSizing&) = delete;
        PopupSizing& operator=(const PopupSizing&) = delete;
    private:
        float ratio{};
    };
}
