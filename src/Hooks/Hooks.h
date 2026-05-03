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
        static void SetMenuVisible(bool visible);
        static bool HasGameWindowFocus();
        static HWND GetGameWindow();
        static void SetModalDialogActive(bool active);
        static bool IsModalDialogActive();
        static HRESULT __stdcall PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);

    private:
        static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static void AttachWindowHook(HWND hwnd);
        static void UpdateCursorState();
        static void UpdateGamePause();
        static void UpdateMenuGodMode();
        static void UpdateHUDVisibility();

        static inline bool menuVisible{ false };
        static inline bool cursorShowing{ false };
        static inline bool ignoreInputManaged{ false };
        static inline bool pauseStateManaged{ false };
        static inline bool freezeTimeWasEnabledBeforeMenu{ false };
        static inline bool godModeStateManaged{ false };
        static inline bool godModeWasEnabledBeforeMenu{ false };
        static inline bool hudVisibilityManaged{ false };
        static inline bool hudWasVisibleBeforeHide{ false };
        static inline bool gameWindowHasFocus{ true };
        static inline bool modalDialogActive{ false };
        static inline HWND gameWindow{ nullptr };
        static inline WNDPROC originalWndProc{ nullptr };
        static inline HRESULT(__stdcall* originalPresent)(IDXGISwapChain*, UINT, UINT){ nullptr };
    };
}
