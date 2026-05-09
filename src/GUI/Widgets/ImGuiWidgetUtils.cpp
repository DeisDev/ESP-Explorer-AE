#include "GUI/Widgets/ImGuiWidgetUtils.h"

#include <algorithm>

namespace ESPExplorerAE::ImGuiWidgetUtils
{
    void DrawWrappedBullet(std::string_view text)
    {
        ImGui::Bullet();
        ImGui::SameLine();
        const float wrapPos = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        ImGui::PushTextWrapPos(wrapPos);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
    }

    bool DrawWrappedButton(const char* label, bool& firstInRow)
    {
        const auto& style = ImGui::GetStyle();
        const float desiredWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;

        if (!firstInRow) {
            const float needed = style.ItemSpacing.x + desiredWidth;
            if (ImGui::GetContentRegionAvail().x >= needed) {
                ImGui::SameLine();
            }
        }

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float clampedWidth = (std::min)(desiredWidth, availableWidth);
        const bool pressed = ImGui::Button(label, ImVec2(clampedWidth, 0.0f));
        firstInRow = false;
        return pressed;
    }

    bool DrawFixedGridButton(const char* label, bool& firstInRow, FixedGridButtonRow& row, int buttonsPerRow, float minButtonWidth)
    {
        const auto& style = ImGui::GetStyle();

        if (firstInRow) {
            row.buttonsInRow = 0;
            row.buttonWidth = 0.0f;
        }

        if (row.buttonsInRow == 0) {
            const float rowAvailable = ImGui::GetContentRegionAvail().x;
            row.buttonWidth = (std::max)(minButtonWidth, (rowAvailable - style.ItemSpacing.x * static_cast<float>(buttonsPerRow - 1)) / static_cast<float>(buttonsPerRow));
        }

        if (row.buttonsInRow > 0) {
            ImGui::SameLine();
        }

        const bool pressed = ImGui::Button(label, ImVec2(row.buttonWidth, 0.0f));
        ++row.buttonsInRow;
        if (row.buttonsInRow >= buttonsPerRow) {
            row.buttonsInRow = 0;
            firstInRow = true;
        } else {
            firstInRow = false;
        }

        return pressed;
    }

    void DrawWrappedSameLine(const char* label)
    {
        const auto& style = ImGui::GetStyle();
        const float nextWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
        const float needed = style.ItemSpacing.x + nextWidth;
        if (ImGui::GetContentRegionAvail().x >= needed) {
            ImGui::SameLine();
        }
    }

    void ShowGameplayDisabledTooltip(bool gameplayActionsAllowed, const char* tooltip)
    {
        if (!gameplayActionsAllowed && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", tooltip);
        }
    }
}
