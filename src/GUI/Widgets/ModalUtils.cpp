#include "GUI/Widgets/ModalUtils.h"

#include <cmath>

namespace ESPExplorerAE::ModalUtils
{
    namespace
    {
        void KeepAspectRatio(ImGuiSizeCallbackData* data)
        {
            const auto* ratioData = static_cast<const float*>(data->UserData);
            if (!ratioData || *ratioData <= 0.0f) {
                return;
            }

            ImVec2 desired = data->DesiredSize;
            const float widthFromHeight = desired.y * *ratioData;
            const float heightFromWidth = desired.x / *ratioData;

            if (std::fabs(widthFromHeight - desired.x) < std::fabs(heightFromWidth - desired.y)) {
                desired.x = widthFromHeight;
            } else {
                desired.y = heightFromWidth;
            }

            data->DesiredSize = desired;
        }
    }

    PopupSizing::PopupSizing(const ImVec2& initialSize, const ImVec2& minSize, const ImVec2& maxSize, bool keepAspectRatio)
    {
        ImGui::SetNextWindowSize(initialSize, ImGuiCond_Appearing);
        if (!keepAspectRatio || initialSize.x <= 0.0f || initialSize.y <= 0.0f) {
            ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
            return;
        }

        ratio = initialSize.x / initialSize.y;
        ImGui::SetNextWindowSizeConstraints(minSize, maxSize, KeepAspectRatio, &ratio);
    }
}