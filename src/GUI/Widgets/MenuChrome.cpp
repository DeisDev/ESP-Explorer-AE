#include "GUI/Widgets/MenuChrome.h"

#include "GUI/Icons.h"
#include "Localization/FontManager.h"

#include <imgui.h>
#include <algorithm>

namespace ESPExplorerAE::MenuChrome
{
    namespace
    {
        void Icon(ImDrawList* draw, ImVec2 position, float size, const char* icon, ImU32 color)
        {
            if (auto* font = FontManager::GetIconFont()) {
                draw->AddText(font, size, position, color, icon);
            }
        }
    }

    bool Header(const char* title, const char* subtitle, const char* closeLabel)
    {
        const float font = ImGui::GetFontSize();
        const float height = font * 3.0f;
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 start = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton("##WindowDrag", { (std::max)(1.0f, width - height), height });
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const auto delta = ImGui::GetIO().MouseDelta;
            const auto position = ImGui::GetWindowPos();
            ImGui::SetWindowPos({ position.x + delta.x, position.y + delta.y });
        }
        const float mark = font * 1.8f;
        const ImVec2 markPos(start.x, start.y + font * 0.35f);
        auto accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        draw->AddRectFilled(markPos, { markPos.x + mark, markPos.y + mark }, ImGui::GetColorU32({ accent.x, accent.y, accent.z, 0.12f }), 8.0f);
        Icon(draw, { markPos.x + mark * 0.23f, markPos.y + mark * 0.23f }, mark * 0.58f, Icons::LayoutGrid, ImGui::GetColorU32(accent));
        const ImVec4 clip(start.x, start.y, start.x + width - height, start.y + height);
        draw->AddText(ImGui::GetFont(), font * 1.18f, { start.x + mark + font * 0.75f, start.y + font * 0.23f },
            ImGui::GetColorU32(ImGuiCol_Text), title, nullptr, 0, &clip);
        draw->AddText(ImGui::GetFont(), font * 0.82f, { start.x + mark + font * 0.75f, start.y + font * 1.65f },
            ImGui::GetColorU32(ImGuiCol_TextDisabled), subtitle, nullptr, 0, &clip);
        ImGui::SameLine(0, 0);
        ImGui::SetCursorScreenPos({ start.x + width - height * 0.65f, start.y + font * 0.3f });
        ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
        const bool close = ImGui::Button(FontManager::GetIconFont() ? "##CloseExplorer" : "x##CloseExplorer", { height * 0.6f, height * 0.6f });
        ImGui::PopStyleColor();
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        const float iconSize = (max.x - min.x) * 0.5f;
        Icon(draw, { (min.x + max.x - iconSize) * 0.5f, (min.y + max.y - iconSize) * 0.5f },
            iconSize, Icons::X, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)) ImGui::SetTooltip("%s", closeLabel);
        ImGui::SetCursorScreenPos({ start.x, start.y + height });
        ImGui::Separator();
        ImGui::Spacing();
        return close;
    }

    void Section(const char* label)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", label);
        ImGui::Spacing();
    }

    bool NavigationItem(const char* id, const char* label, const char* count, const char* icon, bool selected)
    {
        ImGui::PushID(id);
        const float font = ImGui::GetFontSize();
        const ImVec2 size((std::max)(1.0f, ImGui::GetContentRegionAvail().x), font * 1.9f);
        const auto start = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImGui::GetStyleColorVec4(ImGuiCol_Header) : ImVec4(0, 0, 0, 0));
        const bool pressed = ImGui::Button("##Navigate", size);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        auto* draw = ImGui::GetWindowDrawList();
        const auto textColor = ImGui::GetColorU32(selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        const float iconSize = font * 1.05f;
        Icon(draw, { start.x + font * 0.68f, start.y + (size.y - iconSize) * 0.5f }, iconSize, icon,
            ImGui::GetColorU32(selected ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled));
        const ImVec4 clip(start.x + font * 2.1f, start.y, start.x + size.x - font * 0.7f, start.y + size.y);
        draw->AddText(ImGui::GetFont(), font * 0.90f, { clip.x, start.y + (size.y - font * 0.90f) * 0.5f }, textColor, label, nullptr, 0, &clip);
        if (selected) draw->AddRectFilled({ start.x, start.y + font * 0.6f }, { start.x + 3.0f, start.y + size.y - font * 0.6f }, ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0f);
        if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)) {
            if (count && *count) ImGui::SetTooltip("%s (%s)", label, count);
            else ImGui::SetTooltip("%s", label);
        }
        ImGui::PopID();
        return pressed;
    }

    void PageHeading(const char* label, const char* count)
    {
        ImGui::TextUnformatted(label);
        if (count && *count) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", count);
        }
        ImGui::Spacing();
    }
}
