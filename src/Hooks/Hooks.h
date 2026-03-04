#pragma once

#include "pch.h"

#include <d3d11.h>
#include <dxgi.h>

namespace ESPExplorerAE
{
    class Hooks
    {
    public:
        static void Install();
        static bool IsMenuVisible();
        static HWND GetGameWindow();
        static void SetModalDialogActive(bool active);
        static bool IsModalDialogActive();
        static HRESULT __stdcall PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);

    private:
        static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static void AttachWindowHook(HWND hwnd);
        static void UpdateMenuInputState();
        static void UpdateGamePause();

        static inline bool menuVisible{ false };
        static inline bool cursorReleased{ false };
        static inline bool modalDialogActive{ false };
        static inline HWND gameWindow{ nullptr };
        static inline WNDPROC originalWndProc{ nullptr };
        static inline HRESULT(__stdcall* originalPresent)(IDXGISwapChain*, UINT, UINT){ nullptr };
    };
}
