#include "GUI/ImGuiRenderer.h"
#include "GUI/MenuStyle.h"

#include "Config/Config.h"
#include "Core/ScopeExit.h"
#include "Localization/FontManager.h"

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

    }

    bool ImGuiRenderer::Initialize(IDXGISwapChain* a_swapChain, HWND hwnd)
    {
        try {
            return Resources().Initialize(a_swapChain, hwnd, [] {
                ImGui::StyleColorsDark();
                const auto& settings = Config::Get();
                if (settings.enableGamepadNav) ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
                MenuStyle::Apply(settings);
                FontManager::SetCurrentSizeIndex(FontManager::FindClosestSizeIndex(settings.fontSize));
                if (!FontManager::BuildAll()) {
                    REX::WARN("Initial font atlas build failed");
                    return false;
                }
                return true;
            }, &FontManager::ResetAtlasState);
        } catch (const std::exception& error) {
            REX::WARN("Renderer initialization rolled back: {}", error.what());
            return false;
        }
    }

    void ImGuiRenderer::BeginFrame()
    {
        if (!Resources().Ready()) {
            return;
        }

        static bool themeInitialized = false;
        static ThemeState lastTheme{};

        FontManager::EnsureCurrentFontBuilt();

        if (FontManager::HasPendingRebuild()) {
            REX::INFO("{}", "Processing pending font rebuild");
            ImGui_ImplDX11_InvalidateDeviceObjects();
            const bool rebuildSucceeded = FontManager::ProcessPendingRebuild();
            ImGui_ImplDX11_CreateDeviceObjects();
            if (rebuildSucceeded) {
                REX::INFO("{}", "Font rebuild completed successfully");
            } else {
                REX::WARN("{}", "Font rebuild failed");
            }
        }

        const auto& settings = Config::Get();
        const auto currentTheme = BuildThemeState(settings);
        if (!themeInitialized || !SameTheme(lastTheme, currentTheme)) {
            MenuStyle::Apply(settings);
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

        if (auto* swapChain = Resources().SwapChain()) {
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
        if (!Resources().Ready()) {
            return;
        }

        if (fontPushed) {
            ImGui::PopFont();
            fontPushed = false;
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    RendererResources& ImGuiRenderer::Resources()
    {
        // Explicit render-owner release is the only teardown path. If the host
        // terminates without another Present, leave references to process exit;
        // COM/ImGui teardown under the loader lock is not a safe fallback.
        static auto* resources = new RendererResources;
        return *resources;
    }

    void ImGuiRenderer::ReleaseResources()
    {
        Resources().Release();
        fontPushed = false;
    }

    void ImGuiRenderer::Shutdown()
    {
        Config::FlushPendingSave();
        ReleaseResources();
    }

    bool ImGuiRenderer::IsInitialized()
    {
        return Resources().Ready();
    }
}
