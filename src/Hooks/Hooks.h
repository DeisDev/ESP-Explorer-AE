#pragma once

#include "pch.h"

#include <d3d11.h>
#include <dxgi.h>
#include <atomic>

namespace ESPExplorerAE
{
    class Hooks
    {
    public:
        static void Install();
        // Called by the game task only after game/render cleanup is acknowledged.
        static bool RestoreForShutdown();
        static bool IsMenuVisible();
        static void SetMenuVisible(bool visible);
        static bool HasGameWindowFocus();
        static HWND GetGameWindow();
        static void SetModalDialogActive(bool active);
        static bool IsModalDialogActive();
        static HRESULT __stdcall PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);

    private:
        static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static void UpdateCursorState();

        static inline bool cursorShowing{ false };
        static inline std::atomic<HWND> gameWindow{ nullptr };
        static inline WNDPROC originalWndProc{ nullptr };
        using PresentFunction = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
        static inline PresentFunction originalPresent{};
    };
}
