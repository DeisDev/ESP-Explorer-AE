#pragma once

#include <imgui.h>

#include <string>
#include <string_view>
#include <vector>

namespace ESPExplorerAE
{
    class SharedUtils
    {
    public:
        static bool ContainsCaseInsensitive(std::string_view text, std::string_view query);
        static bool EqualsCaseInsensitive(std::string_view left, std::string_view right);
        static bool ContainsByMode(std::string_view text, std::string_view query, bool caseSensitive);
        static std::string BuildParenthesizedList(const std::vector<std::string>& values);
        static void DrawCurrentItemChrome(bool active, bool hovered, bool accentTop, bool accentLeft);
        static void DrawSectionLabel(const char* label);

        static bool DrawWrappedButton(const char* label, bool& firstInRow)
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
    };
}
