#pragma once

#include <imgui.h>

namespace ESPExplorerAE::ModalUtils
{
    void SetNextPopupWindowSizing(const ImVec2& initialSize, const ImVec2& minSize, const ImVec2& maxSize, bool keepAspectRatio = true);
}