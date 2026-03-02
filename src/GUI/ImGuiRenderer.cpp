#include "GUI/ImGuiRenderer.h"

#include "Config/Config.h"
#include "Localization/FontManager.h"
#include "Localization/Language.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>

namespace ESPExplorerAE
{
    namespace
    {
        ImVec4 MulColor(const ImVec4& color, float scale, float alphaScale = 1.0f)
        {
            return ImVec4(
                std::clamp(color.x * scale, 0.0f, 1.0f),
                std::clamp(color.y * scale, 0.0f, 1.0f),
                std::clamp(color.z * scale, 0.0f, 1.0f),
                std::clamp(color.w * alphaScale, 0.0f, 1.0f));
        }

        void ApplyTheme(const Settings& settings)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            auto& colors = style.Colors;

            const ImVec4 accent{ settings.themeAccentR, settings.themeAccentG, settings.themeAccentB, settings.themeAccentA };
            const ImVec4 window{ settings.themeWindowR, settings.themeWindowG, settings.themeWindowB, settings.themeWindowA };
            const ImVec4 panel{ settings.themePanelR, settings.themePanelG, settings.themePanelB, settings.themePanelA };

            colors[ImGuiCol_Text] = MulColor(accent, 1.05f);
            colors[ImGuiCol_TextDisabled] = MulColor(accent, 0.60f);
            colors[ImGuiCol_WindowBg] = window;
            colors[ImGuiCol_ChildBg] = panel;
            colors[ImGuiCol_PopupBg] = MulColor(panel, 0.95f, 1.0f);
            colors[ImGuiCol_Border] = MulColor(accent, 0.55f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

            colors[ImGuiCol_FrameBg] = MulColor(panel, 0.75f);
            colors[ImGuiCol_FrameBgHovered] = MulColor(accent, 0.45f);
            colors[ImGuiCol_FrameBgActive] = MulColor(accent, 0.65f);

            colors[ImGuiCol_TitleBg] = MulColor(window, 0.95f);
            colors[ImGuiCol_TitleBgActive] = MulColor(accent, 0.45f);

            colors[ImGuiCol_Button] = MulColor(accent, 0.55f);
            colors[ImGuiCol_ButtonHovered] = MulColor(accent, 0.75f);
            colors[ImGuiCol_ButtonActive] = MulColor(accent, 0.90f);

            colors[ImGuiCol_Header] = MulColor(accent, 0.40f);
            colors[ImGuiCol_HeaderHovered] = MulColor(accent, 0.66f);
            colors[ImGuiCol_HeaderActive] = MulColor(accent, 0.82f);

            colors[ImGuiCol_CheckMark] = MulColor(accent, 1.0f);
            colors[ImGuiCol_SliderGrab] = MulColor(accent, 0.85f);
            colors[ImGuiCol_SliderGrabActive] = MulColor(accent, 1.0f);

            colors[ImGuiCol_Separator] = MulColor(accent, 0.50f);
            colors[ImGuiCol_SeparatorHovered] = MulColor(accent, 0.78f);
            colors[ImGuiCol_SeparatorActive] = MulColor(accent, 0.95f);

            colors[ImGuiCol_Tab] = MulColor(panel, 0.90f);
            colors[ImGuiCol_TabHovered] = MulColor(accent, 0.64f);
            colors[ImGuiCol_TabActive] = MulColor(accent, 0.78f);
            colors[ImGuiCol_TabUnfocused] = MulColor(panel, 0.80f);
            colors[ImGuiCol_TabUnfocusedActive] = MulColor(accent, 0.58f);

            colors[ImGuiCol_MenuBarBg] = MulColor(panel, 0.90f);
            colors[ImGuiCol_TableHeaderBg] = MulColor(panel, 0.82f);
            colors[ImGuiCol_TableBorderStrong] = MulColor(accent, 0.60f);
            colors[ImGuiCol_TableBorderLight] = MulColor(accent, 0.35f);
            colors[ImGuiCol_TableRowBgAlt] = MulColor(panel, 1.08f);

            colors[ImGuiCol_NavHighlight] = MulColor(accent, 1.0f);
            colors[ImGuiCol_DragDropTarget] = MulColor(accent, 1.0f);
        }
    }

    bool ImGuiRenderer::Initialize(IDXGISwapChain* swapChain, HWND hwnd)
    {
        if (initialized) {
            return true;
        }

        if (!swapChain || !hwnd) {
            return false;
        }

        if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device)))) {
            return false;
        }

        device->GetImmediateContext(&context);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        const auto& settings = Config::Get();
        ApplyTheme(settings);
        FontManager::Build(settings.fontSize, Language::GetCurrentLanguageCode());

        if (!ImGui_ImplWin32_Init(hwnd)) {
            return false;
        }

        if (!ImGui_ImplDX11_Init(device, context)) {
            return false;
        }

        initialized = true;
        return true;
    }

    void ImGuiRenderer::BeginFrame()
    {
        if (!initialized) {
            return;
        }

        ApplyTheme(Config::Get());

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiRenderer::EndFrame()
    {
        if (!initialized) {
            return;
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiRenderer::Shutdown()
    {
        if (!initialized) {
            return;
        }

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

        initialized = false;
    }

    bool ImGuiRenderer::IsInitialized()
    {
        return initialized;
    }
}
