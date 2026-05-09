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
            return text.find(query) != std::string::npos;
        }

        return ContainsCaseInsensitive(text, query);
    }

    void SharedUtils::DrawCurrentItemChrome(bool active, bool hovered, bool accentTop, bool accentLeft)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();

        ImVec4 fill = ImGui::GetStyleColorVec4(active ? ImGuiCol_HeaderActive : (hovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg));
        fill.w = active ? 0.22f : (hovered ? 0.14f : 0.06f);

        ImVec4 border = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        border.w = active ? 1.0f : (hovered ? 0.80f : 0.62f);

        ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        accent.w = active ? 1.0f : 0.72f;

        const ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(fill);
        const ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(border);
        const ImU32 accentColor = ImGui::ColorConvertFloat4ToU32(accent);

        drawList->AddRectFilled(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(max.x - 1.0f, max.y - 1.0f), fillColor);
        drawList->AddRect(min, max, borderColor, 0.0f, 0, active ? 2.0f : 1.0f);

        if (accentTop) {
            drawList->AddLine(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(max.x - 1.0f, min.y + 1.0f), accentColor, active ? 2.5f : 1.5f);
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

        ImVec4 fill = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        fill.w = 0.10f;
        ImVec4 border = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        border.w = 0.88f;
        ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        accent.w = 0.96f;

        drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(fill));
        drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(border), 0.0f, 0, 1.0f);
        drawList->AddLine(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(min.x + 1.0f, max.y - 1.0f), ImGui::ColorConvertFloat4ToU32(accent), 2.5f);

        const ImVec2 textPos = ImVec2(
            min.x + style.FramePadding.x + 8.0f,
            min.y + (height - textSize.y) * 0.5f);
        drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);
    }
}
