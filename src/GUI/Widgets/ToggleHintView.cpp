#include "GUI/Widgets/ToggleHintView.h"

#include <imgui.h>
#include <algorithm>
#include <string>

namespace ESPExplorerAE::ToggleHintView
{
    void Draw(std::string_view key, float opacity,
        const std::function<const char*(std::string_view, std::string_view, const char*)>& localize)
    {
        if (opacity <= 0.0f) return;
        std::string message = localize("General", "sToggleMenuHint", "Toggle ESP Explorer with {key}");
        for (std::size_t offset = 0; (offset = message.find("{key}", offset)) != std::string::npos; offset += key.size()) {
            message.replace(offset, 5, key);
        }

        const auto* viewport = ImGui::GetMainViewport();
        const auto fontSize = ImGui::GetFontSize();
        const auto margin = (std::min)(fontSize, (std::min)(viewport->WorkSize.x, viewport->WorkSize.y) * 0.05f);
        const auto width = (std::max)(1.0f, (std::min)(30.0f * fontSize, viewport->WorkSize.x - 2.0f * margin));
        const auto padding = fontSize * 0.75f;
        ImGui::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x - margin,
            viewport->WorkPos.y + viewport->WorkSize.y - margin }, ImGuiCond_Always, { 1.0f, 1.0f });
        ImGui::SetNextWindowSizeConstraints({ 0.0f, 0.0f }, { width, viewport->WorkSize.y - 2.0f * margin });
        ImGui::SetNextWindowBgAlpha(0.94f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { padding, padding });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, fontSize * 0.4f);
        constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
        if (ImGui::Begin("##ToggleMenuHint", nullptr, flags)) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (std::max)(1.0f, width - 2.0f * padding));
            ImGui::TextUnformatted(message.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
    }
}
