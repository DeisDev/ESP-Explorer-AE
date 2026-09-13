#include "GUI/Widgets/ImGuiWidgetUtils.h"

#include <algorithm>

namespace ESPExplorerAE::ImGuiWidgetUtils
{
    float PaneDividerSize()
    {
        return (std::max)(10.0f, ImGui::GetFontSize() * 0.65f);
    }

    void PaneDivider(const char* id, float& leadingSize, float minimum, float maximum, const char* tooltip, bool vertical)
    {
        const float thickness = PaneDividerSize();
        const auto available = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton(id, vertical ? ImVec2(thickness, (std::max)(1.0f, available.y)) : ImVec2((std::max)(1.0f, available.x), thickness));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if (hovered || active) ImGui::SetMouseCursor(vertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
        if (active) leadingSize = std::clamp(leadingSize + (vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y), minimum, maximum);
        if (hovered && tooltip) ImGui::SetTooltip("%s", tooltip);
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const float half = (std::min)(ImGui::GetFontSize() * 1.5f, (vertical ? max.y - min.y : max.x - min.x) * 0.35f);
        auto* draw = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_TextDisabled);
        draw->AddRectFilled(vertical ? ImVec2(center.x - 1.5f, center.y - half) : ImVec2(center.x - half, center.y - 1.5f),
            vertical ? ImVec2(center.x + 1.5f, center.y + half) : ImVec2(center.x + half, center.y + 1.5f), color, 2.0f);
    }

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
        const float desiredWidth = ImGui::CalcTextSize(label, nullptr, true).x + style.FramePadding.x * 2.0f;

        if (!firstInRow) {
            SameLineIfFits(desiredWidth);
        }

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float clampedWidth = (std::max)(1.0f, (std::min)(desiredWidth, availableWidth));
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
        SameLineIfFits(nextWidth + ImGui::GetFrameHeight());
    }

    void SameLineIfFits(float width)
    {
        const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        if (right - ImGui::GetItemRectMax().x - ImGui::GetStyle().ItemSpacing.x >= width) {
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
