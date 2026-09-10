#include "GUI/Widgets/SharedUtils.h"

#include <algorithm>
#include <cctype>

namespace ESPExplorerAE
{
    namespace
    {
        char FoldCase(unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        }
    }

    bool SharedUtils::ContainsCaseInsensitive(std::string_view text, std::string_view query)
    {
        if (query.empty()) {
            return true;
        }

        const auto match = std::search(text.begin(), text.end(), query.begin(), query.end(), [](char left, char right) {
            return FoldCase(static_cast<unsigned char>(left)) == FoldCase(static_cast<unsigned char>(right));
        });

        return match != text.end();
    }

    bool SharedUtils::EqualsCaseInsensitive(std::string_view left, std::string_view right)
    {
        return left.size() == right.size() &&
               std::equal(left.begin(), left.end(), right.begin(), [](char lhs, char rhs) {
                   return FoldCase(static_cast<unsigned char>(lhs)) == FoldCase(static_cast<unsigned char>(rhs));
               });
    }

    bool SharedUtils::ContainsByMode(std::string_view text, std::string_view query, bool caseSensitive)
    {
        if (query.empty()) {
            return true;
        }

        if (caseSensitive) {
            return text.find(query) != std::string_view::npos;
        }

        return ContainsCaseInsensitive(text, query);
    }

    void SharedUtils::DrawCurrentItemChrome(bool active, bool hovered, bool accentTop, bool accentLeft)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();

        if (!active && !hovered) return;
        const ImU32 accentColor = ImGui::GetColorU32(active ? ImGuiCol_CheckMark : ImGuiCol_Border);

        if (accentTop) {
            drawList->AddLine(ImVec2(min.x + 5.0f, max.y - 1.0f), ImVec2(max.x - 5.0f, max.y - 1.0f), accentColor, active ? 2.0f : 1.0f);
        }

        if (accentLeft) {
            drawList->AddLine(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(min.x + 1.0f, max.y - 1.0f), accentColor, active ? 2.5f : 1.5f);
        }
    }

    void SharedUtils::DrawSectionLabel(const char* label)
    {
        const auto& style = ImGui::GetStyle();
        const float width = ImGui::GetContentRegionAvail().x;
        const float height = ImGui::GetFrameHeight();
        const ImVec2 start = ImGui::GetCursorScreenPos();

        ImGui::Dummy(ImVec2(width, height));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = start;
        const ImVec2 max = ImVec2(start.x + width, start.y + height);
        const ImVec2 textSize = ImGui::CalcTextSize(label);

        drawList->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(ImGuiCol_Separator));

        const ImVec2 textPos = ImVec2(
            min.x + style.FramePadding.x,
            min.y + (height - textSize.y) * 0.5f);
        drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);
    }
}
