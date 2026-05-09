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
    void ShowGameplayDisabledTooltip(bool gameplayActionsAllowed, const char* tooltip);
}
