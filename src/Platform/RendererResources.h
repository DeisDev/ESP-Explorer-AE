#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

struct ImGuiContext;
struct ImDrawData;

namespace ESPExplorerAE
{
    enum class RendererStage { Device, ImmediateContext, ImGuiContext, Prepared, Win32, DX11, DeviceObjects };

    // Owns only platform/backend resources. Preparation/reset callbacks let the
    // UI own fonts and style; stage checkpoints allow real Windows failure tests.
    // Calls belong to the render thread, including explicit Release().
    class RendererResources
    {
    public:
        RendererResources() = default;
        RendererResources(const RendererResources&) = delete;
        RendererResources& operator=(const RendererResources&) = delete;
        bool Initialize(IDXGISwapChain* swapChain, HWND window, bool (*prepare)() = nullptr,
            void (*reset)() = nullptr, bool (*checkpoint)(RendererStage) = nullptr);
        HRESULT RenderDrawData(ImDrawData* drawData);
        void Release();
        bool Ready() const { return ready; }
        ImGuiContext* Context() const { return imgui; }
        IDXGISwapChain* SwapChain() const { return swapChain.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate;
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
        ImGuiContext* imgui{};
        void (*reset)(){};
        bool win32Ready{};
        bool dx11Ready{};
        bool ready{};
    };
}
