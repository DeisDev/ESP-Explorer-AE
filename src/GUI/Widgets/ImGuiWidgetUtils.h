#pragma once

#include <imgui.h>

#include <string_view>

namespace ESPExplorerAE::ImGuiWidgetUtils
{
    // Keep transient feedback from changing the position of adjacent controls.
    // The text-only callback also supplies the full message tooltip; long
    // localized messages remain scrollable without taking space from results.
    template <class Draw>
    void DrawStatusArea(const char* id, Draw&& draw)
    {
        bool hasContent = false;
        const auto spacing = ImGui::GetStyle().ItemSpacing;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { spacing.x, spacing.y * 0.5f });
        if (ImGui::BeginChild(id, { 0.0f, ImGui::GetTextLineHeight() })) {
            const float start = ImGui::GetCursorPosY();
            draw();
            hasContent = ImGui::GetCursorPosY() > start;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        if (hasContent && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            draw();
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    struct FixedGridButtonRow
    {
        int buttonsInRow{ 0 };
        float buttonWidth{ 0.0f };
    };

    void DrawWrappedBullet(std::string_view text);
    bool DrawWrappedButton(const char* label, bool& firstInRow);
    bool DrawFixedGridButton(const char* label, bool& firstInRow, FixedGridButtonRow& row, int buttonsPerRow = 3, float minButtonWidth = 96.0f);
    void DrawWrappedSameLine(const char* label);
    void SameLineIfFits(float width);
    float PaneDividerSize();
    void PaneDivider(const char* id, float& leadingSize, float minimum, float maximum, const char* tooltip, bool vertical = true);
    void ShowGameplayDisabledTooltip(bool gameplayActionsAllowed, const char* tooltip);
}
