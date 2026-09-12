#include "GUI/MenuStyle.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace ESPExplorerAE::MenuStyle
{
    namespace
    {
        ImVec4 Mix(ImVec4 a, ImVec4 b, float amount)
        {
            return { std::lerp(a.x, b.x, amount), std::lerp(a.y, b.y, amount),
                std::lerp(a.z, b.z, amount), std::lerp(a.w, b.w, amount) };
        }
    }

    void Apply(const Settings& settings)
    {
        auto& style = ImGui::GetStyle();
        auto& colors = style.Colors;
        const ImVec4 accent{ settings.themeAccentR, settings.themeAccentG, settings.themeAccentB, settings.themeAccentA };
        const ImVec4 window{ settings.themeWindowR, settings.themeWindowG, settings.themeWindowB, settings.themeWindowA };
        const ImVec4 panel{ settings.themePanelR, settings.themePanelG, settings.themePanelB, settings.themePanelA };
        const float brightness = panel.x * 0.2126f + panel.y * 0.7152f + panel.z * 0.0722f;
        const ImVec4 text = brightness > 0.55f ? ImVec4(0.09f, 0.11f, 0.14f, 1.0f) : ImVec4(0.89f, 0.91f, 0.94f, 1.0f);
        const ImVec4 frame = Mix(panel, text, 0.045f);
        const ImVec4 border = Mix(panel, text, 0.13f);

        style.Alpha = std::clamp(settings.windowAlpha, 0.0f, 1.0f);
        colors[ImGuiCol_Text] = text;
        colors[ImGuiCol_TextDisabled] = Mix(panel, text, 0.58f);
        colors[ImGuiCol_WindowBg] = window;
        colors[ImGuiCol_ChildBg] = panel;
        colors[ImGuiCol_PopupBg] = Mix(panel, text, 0.025f);
        colors[ImGuiCol_Border] = border;
        colors[ImGuiCol_BorderShadow] = { 0, 0, 0, 0 };
        colors[ImGuiCol_FrameBg] = frame;
        colors[ImGuiCol_FrameBgHovered] = Mix(frame, accent, 0.10f);
        colors[ImGuiCol_FrameBgActive] = Mix(frame, accent, 0.16f);
        colors[ImGuiCol_TitleBg] = window;
        colors[ImGuiCol_TitleBgActive] = panel;
        colors[ImGuiCol_TitleBgCollapsed] = window;
        // Overlapping popup surfaces and title bars must cover the text below.
        // The user's overall interface opacity still applies through style.Alpha.
        for (const auto color : { ImGuiCol_PopupBg, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_TitleBgCollapsed })
            colors[color].w = 1.0f;
        colors[ImGuiCol_Button] = Mix(panel, text, 0.055f);
        colors[ImGuiCol_ButtonHovered] = Mix(panel, accent, 0.18f);
        colors[ImGuiCol_ButtonActive] = Mix(panel, accent, 0.28f);
        colors[ImGuiCol_Header] = Mix(panel, accent, 0.14f);
        colors[ImGuiCol_HeaderHovered] = Mix(panel, accent, 0.20f);
        colors[ImGuiCol_HeaderActive] = Mix(panel, accent, 0.26f);
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = Mix(accent, text, 0.12f);
        colors[ImGuiCol_SliderGrabActive] = accent;
        colors[ImGuiCol_Separator] = border;
        colors[ImGuiCol_SeparatorHovered] = Mix(panel, accent, 0.5f);
        colors[ImGuiCol_SeparatorActive] = accent;
        colors[ImGuiCol_Tab] = frame;
        colors[ImGuiCol_TabHovered] = Mix(panel, accent, 0.18f);
        colors[ImGuiCol_TabSelected] = Mix(panel, accent, 0.13f);
        colors[ImGuiCol_TabSelectedOverline] = accent;
        colors[ImGuiCol_TabDimmed] = window;
        colors[ImGuiCol_TabDimmedSelected] = frame;
        colors[ImGuiCol_TabDimmedSelectedOverline] = border;
        colors[ImGuiCol_MenuBarBg] = panel;
        colors[ImGuiCol_TableHeaderBg] = Mix(panel, text, 0.025f);
        colors[ImGuiCol_TableBorderStrong] = border;
        colors[ImGuiCol_TableBorderLight] = Mix(panel, text, 0.055f);
        colors[ImGuiCol_TableRowBg] = { 0, 0, 0, 0 };
        colors[ImGuiCol_TableRowBgAlt] = { text.x, text.y, text.z, 0.018f };
        colors[ImGuiCol_ScrollbarBg] = { 0, 0, 0, 0 };
        colors[ImGuiCol_ScrollbarGrab] = Mix(panel, text, 0.18f);
        colors[ImGuiCol_ScrollbarGrabHovered] = Mix(panel, text, 0.30f);
        colors[ImGuiCol_ScrollbarGrabActive] = Mix(panel, accent, 0.5f);
        colors[ImGuiCol_ResizeGrip] = Mix(panel, text, 0.10f);
        colors[ImGuiCol_ResizeGripHovered] = Mix(panel, accent, 0.45f);
        colors[ImGuiCol_ResizeGripActive] = accent;
        colors[ImGuiCol_NavCursor] = accent;
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_TextSelectedBg] = { accent.x, accent.y, accent.z, 0.28f };
        colors[ImGuiCol_ModalWindowDimBg] = { 0.02f, 0.025f, 0.035f, 0.66f };

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;
        style.WindowRounding = 12.0f;
        style.ChildRounding = 8.0f;
        style.PopupRounding = 8.0f;
        style.FrameRounding = 5.0f;
        style.GrabRounding = 4.0f;
        style.ScrollbarRounding = 8.0f;
        style.TabRounding = 5.0f;
        style.WindowPadding = { 16.0f, 14.0f };
        style.FramePadding = { 11.0f, 7.0f };
        style.ItemSpacing = { 10.0f, 8.0f };
        style.ItemInnerSpacing = { 7.0f, 5.0f };
        style.CellPadding = { 10.0f, 6.0f };
        style.ScrollbarSize = 10.0f;
        style.GrabMinSize = 10.0f;
        style.IndentSpacing = 18.0f;
        style.WindowTitleAlign = { 0.0f, 0.5f };
        style.ButtonTextAlign = { 0.5f, 0.5f };
    }
}
