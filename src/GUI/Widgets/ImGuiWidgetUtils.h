#pragma once

#include <imgui.h>

#include <string_view>

namespace ESPExplorerAE::ImGuiWidgetUtils
{
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
