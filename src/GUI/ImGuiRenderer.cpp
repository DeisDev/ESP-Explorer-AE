#include "GUI/ImGuiRenderer.h"

#include "Config/Config.h"
#include "Localization/FontManager.h"
#include "Localization/Language.h"
#include "Logging/Logger.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>

namespace ESPExplorerAE
{
    namespace
    {
        struct ThemeState
        {
            float windowAlpha{ 0.0f };
            float accentR{ 0.0f };
            float accentG{ 0.0f };
            float accentB{ 0.0f };
            float accentA{ 0.0f };
            float windowR{ 0.0f };
            float windowG{ 0.0f };
            float windowB{ 0.0f };
            float windowA{ 0.0f };
            float panelR{ 0.0f };
            float panelG{ 0.0f };
            float panelB{ 0.0f };
            float panelA{ 0.0f };
        };

        ThemeState BuildThemeState(const Settings& settings)
        {
            return ThemeState{
                .windowAlpha = settings.windowAlpha,
                .accentR = settings.themeAccentR,
                .accentG = settings.themeAccentG,
                .accentB = settings.themeAccentB,
                .accentA = settings.themeAccentA,
                .windowR = settings.themeWindowR,
                .windowG = settings.themeWindowG,
                .windowB = settings.themeWindowB,
                .windowA = settings.themeWindowA,
                .panelR = settings.themePanelR,
                .panelG = settings.themePanelG,
                .panelB = settings.themePanelB,
                .panelA = settings.themePanelA
            };
        }

        bool SameTheme(const ThemeState& left, const ThemeState& right)
        {
            return left.windowAlpha == right.windowAlpha &&
                   left.accentR == right.accentR &&
                   left.accentG == right.accentG &&
                   left.accentB == right.accentB &&
                   left.accentA == right.accentA &&
                   left.windowR == right.windowR &&
                   left.windowG == right.windowG &&
                   left.windowB == right.windowB &&
                   left.windowA == right.windowA &&
                   left.panelR == right.panelR &&
                   left.panelG == right.panelG &&
                   left.panelB == right.panelB &&
                   left.panelA == right.panelA;
        }

        ImVec4 MulColor(const ImVec4& color, float scale, float alphaScale = 1.0f)
        {
            return ImVec4(
                std::clamp(color.x * scale, 0.0f, 1.0f),
                std::clamp(color.y * scale, 0.0f, 1.0f),
                std::clamp(color.z * scale, 0.0f, 1.0f),
                std::clamp(color.w * alphaScale, 0.0f, 1.0f));
        }

        ImVec4 BlendColor(const ImVec4& left, const ImVec4& right, float factor)
        {
            return ImVec4(
                std::lerp(left.x, right.x, factor),
                std::lerp(left.y, right.y, factor),
                std::lerp(left.z, right.z, factor),
                std::lerp(left.w, right.w, factor));
        }

        void ApplyTheme(const Settings& settings)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            auto& colors = style.Colors;

            style.Alpha = std::clamp(settings.windowAlpha, 0.0f, 1.0f);

            const ImVec4 accent{ settings.themeAccentR, settings.themeAccentG, settings.themeAccentB, settings.themeAccentA };
            const ImVec4 window{ settings.themeWindowR, settings.themeWindowG, settings.themeWindowB, settings.themeWindowA };
            const ImVec4 panel{ settings.themePanelR, settings.themePanelG, settings.themePanelB, settings.themePanelA };
            const ImVec4 border = MulColor(accent, 0.78f, 0.92f);
            const ImVec4 highlight = MulColor(accent, 1.0f, 1.0f);
            const ImVec4 frame = BlendColor(window, panel, 0.35f);
            const ImVec4 panelAlt = BlendColor(window, panel, 0.60f);
            const ImVec4 accentWash = MulColor(accent, 0.24f, 0.78f);
            const ImVec4 accentStrong = MulColor(accent, 0.42f, 0.88f);
            const ImVec4 transparentAccent = ImVec4(border.x, border.y, border.z, 0.10f);

            colors[ImGuiCol_Text] = highlight;
            colors[ImGuiCol_TextDisabled] = MulColor(accent, 0.72f);
            colors[ImGuiCol_WindowBg] = window;
            colors[ImGuiCol_ChildBg] = panelAlt;
            colors[ImGuiCol_PopupBg] = BlendColor(window, panel, 0.50f);
            colors[ImGuiCol_Border] = border;
            colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

            colors[ImGuiCol_FrameBg] = frame;
            colors[ImGuiCol_FrameBgHovered] = accentWash;
            colors[ImGuiCol_FrameBgActive] = accentStrong;

            colors[ImGuiCol_TitleBg] = BlendColor(window, panel, 0.18f);
            colors[ImGuiCol_TitleBgActive] = BlendColor(window, panel, 0.32f);
            colors[ImGuiCol_TitleBgCollapsed] = BlendColor(window, panel, 0.12f);

            colors[ImGuiCol_Button] = transparentAccent;
            colors[ImGuiCol_ButtonHovered] = accentWash;
            colors[ImGuiCol_ButtonActive] = accentStrong;

            colors[ImGuiCol_Header] = transparentAccent;
            colors[ImGuiCol_HeaderHovered] = accentWash;
            colors[ImGuiCol_HeaderActive] = accentStrong;

            colors[ImGuiCol_CheckMark] = highlight;
            colors[ImGuiCol_SliderGrab] = highlight;
            colors[ImGuiCol_SliderGrabActive] = MulColor(accent, 0.18f);

            colors[ImGuiCol_Separator] = border;
            colors[ImGuiCol_SeparatorHovered] = highlight;
            colors[ImGuiCol_SeparatorActive] = highlight;

            colors[ImGuiCol_Tab] = frame;
            colors[ImGuiCol_TabHovered] = accentWash;
            colors[ImGuiCol_TabActive] = accentStrong;
            colors[ImGuiCol_TabUnfocused] = BlendColor(window, panel, 0.22f);
            colors[ImGuiCol_TabUnfocusedActive] = BlendColor(accentStrong, frame, 0.45f);

            colors[ImGuiCol_MenuBarBg] = BlendColor(window, panel, 0.28f);
            colors[ImGuiCol_TableHeaderBg] = BlendColor(window, panel, 0.42f);
            colors[ImGuiCol_TableBorderStrong] = border;
            colors[ImGuiCol_TableBorderLight] = MulColor(accent, 0.42f);
            colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_TableRowBgAlt] = BlendColor(window, panel, 0.16f);

            colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_ScrollbarGrab] = transparentAccent;
            colors[ImGuiCol_ScrollbarGrabHovered] = accentWash;
            colors[ImGuiCol_ScrollbarGrabActive] = accentStrong;

            colors[ImGuiCol_ResizeGrip] = transparentAccent;
            colors[ImGuiCol_ResizeGripHovered] = accentWash;
            colors[ImGuiCol_ResizeGripActive] = accentStrong;

            colors[ImGuiCol_NavHighlight] = highlight;
            colors[ImGuiCol_DragDropTarget] = highlight;

            style.WindowBorderSize = 2.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 2.0f;
            style.FrameBorderSize = 1.0f;
            style.TabBorderSize = 1.0f;

            style.WindowRounding = 0.0f;
            style.ChildRounding = 0.0f;
            style.PopupRounding = 0.0f;
            style.FrameRounding = 0.0f;
            style.GrabRounding = 0.0f;
            style.ScrollbarRounding = 0.0f;
            style.TabRounding = 0.0f;

            style.WindowPadding = ImVec2(10.0f, 10.0f);
            style.FramePadding = ImVec2(8.0f, 5.0f);
            style.ItemSpacing = ImVec2(8.0f, 6.0f);
            style.ScrollbarSize = 16.0f;
            style.WindowTitleAlign = ImVec2(0.03f, 0.5f);
        }
    }

    bool ImGuiRenderer::Initialize(IDXGISwapChain* a_swapChain, HWND hwnd)
    {
        if (initialized) {
            return true;
        }

        if (!a_swapChain || !hwnd) {
            return false;
        }

        if (FAILED(a_swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device)))) {
            return false;
        }

        device->GetImmediateContext(&context);
        swapChain = a_swapChain;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        const auto& settings = Config::Get();

        ImGuiIO& io = ImGui::GetIO();
        if (settings.enableGamepadNav) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        }

        ApplyTheme(settings);

        const int sizeIndex = FontManager::FindClosestSizeIndex(settings.fontSize);
        FontManager::SetCurrentSizeIndex(sizeIndex);
        if (!FontManager::BuildAll(Language::GetCurrentLanguageCode())) {
            Logger::Warn("Initial font atlas build failed");
        } else {
            Logger::Info("Initial font atlas built for language " + Language::GetCurrentLanguageCode());
        }

        if (!ImGui_ImplWin32_Init(hwnd)) {
            Logger::Error("ImGui Win32 initialization failed");
            return false;
        }

        if (!ImGui_ImplDX11_Init(device, context)) {
            Logger::Error("ImGui DX11 initialization failed");
            return false;
        }

        initialized = true;
        Logger::Info("ImGui renderer initialized");
        return true;
    }

    void ImGuiRenderer::BeginFrame()
    {
        if (!initialized) {
            return;
        }

        static bool themeInitialized = false;
        static ThemeState lastTheme{};

        Config::FlushPendingSaveIfDue();
        FontManager::EnsureCurrentFontBuilt();

        if (FontManager::HasPendingRebuild()) {
            Logger::Info("Processing pending font rebuild");
            ImGui_ImplDX11_InvalidateDeviceObjects();
            const bool rebuildSucceeded = FontManager::ProcessPendingRebuild();
            ImGui_ImplDX11_CreateDeviceObjects();
            if (rebuildSucceeded) {
                Logger::Info("Font rebuild completed successfully");
            } else {
                Logger::Warn("Font rebuild failed");
            }
        }

        const auto& settings = Config::Get();
        const auto currentTheme = BuildThemeState(settings);
        if (!themeInitialized || !SameTheme(lastTheme, currentTheme)) {
            ApplyTheme(settings);
            lastTheme = currentTheme;
            themeInitialized = true;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (settings.enableGamepadNav) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        if (swapChain) {
            DXGI_SWAP_CHAIN_DESC scDesc{};
            if (SUCCEEDED(swapChain->GetDesc(&scDesc))) {
                ImGuiIO& ioRef = ImGui::GetIO();
                const float bbW = static_cast<float>(scDesc.BufferDesc.Width);
                const float bbH = static_cast<float>(scDesc.BufferDesc.Height);
                if (bbW > 0.0f && bbH > 0.0f && ioRef.DisplaySize.x > 0.0f && ioRef.DisplaySize.y > 0.0f) {
                    ioRef.DisplayFramebufferScale = ImVec2(bbW / ioRef.DisplaySize.x, bbH / ioRef.DisplaySize.y);
                }
            }
        }

        ImGui::NewFrame();

        if (auto* font = FontManager::GetCurrentFont()) {
            ImGui::PushFont(font);
            fontPushed = true;
        }
    }

    void ImGuiRenderer::EndFrame()
    {
        if (!initialized) {
            return;
        }

        if (fontPushed) {
            ImGui::PopFont();
            fontPushed = false;
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiRenderer::Shutdown()
    {
        if (!initialized) {
            return;
        }

        Config::FlushPendingSave();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (context) {
            context->Release();
            context = nullptr;
        }

        if (device) {
            device->Release();
            device = nullptr;
        }

        swapChain = nullptr;
        initialized = false;
    }

    bool ImGuiRenderer::IsInitialized()
    {
        return initialized;
    }
}
