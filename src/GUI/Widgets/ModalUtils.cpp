#include "GUI/Widgets/ModalUtils.h"

#include <cmath>
#include <algorithm>
#include <imgui_internal.h>

namespace ESPExplorerAE::ModalUtils
{
    namespace
    {
        ImVec2 WorkLimit()
        {
            const auto size = ImGui::GetMainViewport()->WorkSize;
            return { (std::max)(1.0f, size.x - 16.0f), (std::max)(1.0f, size.y - 16.0f) };
        }

        ImVec2 ClampSize(ImVec2 size, ImVec2 minimum, ImVec2 maximum)
        {
            return { std::clamp(size.x, minimum.x, maximum.x), std::clamp(size.y, minimum.y, maximum.y) };
        }

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

    bool CanOpenPopup(const char* id)
    {
        return ImGui::IsPopupOpen(id) || !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    }

    bool EscapeClosesCurrentWindow()
    {
        // Escape belongs to a combo or text edit before it belongs to its owner.
        const auto& context = *ImGui::GetCurrentContext();
        if (context.ActiveIdPreviousFrame) return false;
        for (const auto* window : context.Windows) {
            if ((window->Flags & ImGuiWindowFlags_Popup) && window->LastFrameActive >= context.FrameCount - 1) return false;
        }
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
               !ImGui::IsAnyItemActive() &&
               !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
               (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
    }

    bool CancelPopupRequested()
    {
        const auto& context = *ImGui::GetCurrentContext();
        return !context.ActiveIdPreviousFrame && !ImGui::IsAnyItemActive() &&
            context.OpenPopupStack.Size == context.BeginPopupStack.Size &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
    }

    void PrepareToolWindow(const char* name, ImVec2 initialSize, ImVec2 minimumSize,
        bool& focusPending, const ImVec2* initialPosition)
    {
        const auto* viewport = ImGui::GetMainViewport();
        const auto maximum = WorkLimit();
        const auto minimum = ClampSize(minimumSize, { 1, 1 }, maximum);
        const auto* window = ImGui::FindWindowByName(name);
        const auto size = ClampSize(window ? window->SizeFull : initialSize, minimum, maximum);
        auto position = window ? window->Pos : initialPosition ? *initialPosition :
            ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - size.x) * 0.5f,
                viewport->WorkPos.y + (viewport->WorkSize.y - size.y) * 0.5f);
        position.x = std::clamp(position.x, viewport->WorkPos.x + 8.0f, viewport->WorkPos.x + 8.0f + maximum.x - size.x);
        position.y = std::clamp(position.y, viewport->WorkPos.y + 8.0f, viewport->WorkPos.y + 8.0f + maximum.y - size.y);
        ImGui::SetNextWindowSize(size);
        ImGui::SetNextWindowPos(position);
        ImGui::SetNextWindowSizeConstraints(minimum, maximum);
        ImGui::SetNextWindowBgAlpha(1.0f);
        if (focusPending && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
            ImGui::SetNextWindowFocus();
            focusPending = false;
        }
    }

    PopupSizing::PopupSizing(const ImVec2& initialSize, const ImVec2& minSize, const ImVec2& maxSize, bool keepAspectRatio)
    {
        const auto maximum = ClampSize(maxSize, { 1, 1 }, WorkLimit());
        const auto minimum = ClampSize(minSize, { 1, 1 }, maximum);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Appearing, { 0.5f, 0.5f });
        ImGui::SetNextWindowSize(ClampSize(initialSize, minimum, maximum), ImGuiCond_Appearing);
        ImGui::SetNextWindowBgAlpha(1.0f);
        if (!keepAspectRatio || initialSize.x <= 0.0f || initialSize.y <= 0.0f) {
            ImGui::SetNextWindowSizeConstraints(minimum, maximum);
            return;
        }

        ratio = initialSize.x / initialSize.y;
        ImGui::SetNextWindowSizeConstraints(minimum, maximum, KeepAspectRatio, &ratio);
    }
}
