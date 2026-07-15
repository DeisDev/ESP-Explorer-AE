#include "Platform/RendererResources.h"
#include "Core/ScopeExit.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

namespace ESPExplorerAE
{
    bool RendererResources::Initialize(IDXGISwapChain* chain, HWND window, bool (*prepare)(),
        void (*onReset)(), bool (*checkpoint)(RendererStage))
    {
        if (ready) return true;
        if (!chain || !window) return false;
        const auto previous = ImGui::GetCurrentContext();
        ScopeExit restore([previous] { ImGui::SetCurrentContext(previous); });
        ScopeExit rollback([this] { Release(); });
        const auto proceed = [checkpoint](RendererStage stage) { return !checkpoint || checkpoint(stage); };
        if (FAILED(chain->GetDevice(IID_PPV_ARGS(device.GetAddressOf()))) || !proceed(RendererStage::Device)) return false;
        device->GetImmediateContext(immediate.GetAddressOf());
        if (!immediate || !proceed(RendererStage::ImmediateContext)) return false;
        swapChain = chain;
        IMGUI_CHECKVERSION();
        imgui = ImGui::CreateContext();
        if (!imgui) return false;
        ImGui::SetCurrentContext(imgui);
        reset = onReset;
        if (!proceed(RendererStage::ImGuiContext)) return false;
        if ((prepare && !prepare()) || !proceed(RendererStage::Prepared)) return false;
        if (!ImGui_ImplWin32_Init(window)) return false;
        win32Ready = true;
        if (!proceed(RendererStage::Win32)) return false;
        if (!ImGui_ImplDX11_Init(device.Get(), immediate.Get())) return false;
        dx11Ready = true;
        if (!proceed(RendererStage::DX11)) return false;
        if (!ImGui_ImplDX11_CreateDeviceObjects() || !proceed(RendererStage::DeviceObjects)) return false;
        ready = true;
        rollback.Release();
        return true;
    }

    void RendererResources::Release()
    {
        const auto previous = ImGui::GetCurrentContext();
        if (imgui) {
            ImGui::SetCurrentContext(imgui);
            if (dx11Ready) ImGui_ImplDX11_Shutdown();
            if (win32Ready) ImGui_ImplWin32_Shutdown();
            if (reset) reset();
            ImGui::DestroyContext(imgui);
            ImGui::SetCurrentContext(previous == imgui ? nullptr : previous);
            imgui = nullptr;
        }
        reset = nullptr;
        win32Ready = dx11Ready = ready = false;
        swapChain.Reset();
        immediate.Reset();
        device.Reset();
    }
}
