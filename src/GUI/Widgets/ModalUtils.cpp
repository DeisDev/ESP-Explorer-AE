#include "GUI/Widgets/ModalUtils.h"

#include <cmath>

namespace ESPExplorerAE::ModalUtils
{
    namespace
    {
        struct AspectRatioConstraint
        {
            float ratio;
        };

        void KeepAspectRatio(ImGuiSizeCallbackData* data)
        {
            const auto* ratioData = static_cast<const AspectRatioConstraint*>(data->UserData);
            if (!ratioData || ratioData->ratio <= 0.0f) {
                return;
            }

            ImVec2 desired = data->DesiredSize;
            const float widthFromHeight = desired.y * ratioData->ratio;
            const float heightFromWidth = desired.x / ratioData->ratio;

            if (std::fabs(widthFromHeight - desired.x) < std::fabs(heightFromWidth - desired.y)) {
                desired.x = widthFromHeight;
            } else {
                desired.y = heightFromWidth;
            }

            data->DesiredSize = desired;
        }
    }

    void SetNextPopupWindowSizing(const ImVec2& initialSize, const ImVec2& minSize, const ImVec2& maxSize, bool keepAspectRatio)
    {
        ImGui::SetNextWindowSize(initialSize, ImGuiCond_Appearing);
        if (!keepAspectRatio || initialSize.x <= 0.0f || initialSize.y <= 0.0f) {
            ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
            return;
        }

        AspectRatioConstraint ratioConstraint{ initialSize.x / initialSize.y };
        ImGui::SetNextWindowSizeConstraints(minSize, maxSize, KeepAspectRatio, &ratioConstraint);
    }
}