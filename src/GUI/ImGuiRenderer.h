#pragma once

#include "pch.h"

#include <d3d11.h>
#include <dxgi.h>
#include "Platform/RendererResources.h"

struct ImGuiContext;

namespace ESPExplorerAE
{
    class ImGuiRenderer
    {
    public:
        static bool Initialize(IDXGISwapChain* a_swapChain, HWND hwnd);
        static void BeginFrame();
        static void EndFrame();
        static void Shutdown();
        static bool IsInitialized();
        static ImGuiContext* GetContext() { return Resources().Context(); }

    private:
        static void ReleaseResources();
        static RendererResources& Resources();
        static inline bool fontPushed{};
    };
}
