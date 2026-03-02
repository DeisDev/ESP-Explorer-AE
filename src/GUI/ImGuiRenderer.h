#pragma once

#include "pch.h"

#include <d3d11.h>
#include <dxgi.h>

namespace ESPExplorerAE
{
    class ImGuiRenderer
    {
    public:
        static bool Initialize(IDXGISwapChain* swapChain, HWND hwnd);
        static void BeginFrame();
        static void EndFrame();
        static void Shutdown();
        static bool IsInitialized();

    private:
        static inline bool initialized{ false };
        static inline ID3D11Device* device{ nullptr };
        static inline ID3D11DeviceContext* context{ nullptr };
    };
}
