#include "Hooks/Hooks.h"

#include "Config/Config.h"
#include "GUI/ImGuiRenderer.h"
#include "GUI/MainWindow.h"
#include "Input/GamepadInput.h"
#include "Logging/Logger.h"

#include <RE/B/BSGraphics.h>
#include <RE/M/Main.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

#include <array>
#include <cstdlib>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ESPExplorerAE
{
    namespace
    {
        using SetCursorPos_t = BOOL(WINAPI*)(int, int);
        using ClipCursor_t = BOOL(WINAPI*)(const RECT*);

        SetCursorPos_t originalSetCursorPos{ nullptr };
        ClipCursor_t originalClipCursor{ nullptr };

        bool freeCursorPosValid{ false };
        POINT freeCursorPos{ 0, 0 };

        bool IsBlockingGameMenuOpen()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return false;
            }

            static const std::array<RE::BSFixedString, 12> blockingMenuNames{
                RE::BSFixedString("BarterMenu"),
                RE::BSFixedString("ContainerMenu"),
                RE::BSFixedString("DialogueMenu"),
                RE::BSFixedString("LevelUpMenu"),
                RE::BSFixedString("LockpickingMenu"),
                RE::BSFixedString("LooksMenu"),
                RE::BSFixedString("PauseMenu"),
                RE::BSFixedString("PipboyMenu"),
                RE::BSFixedString("PipboyWorkshopMenu"),
                RE::BSFixedString("SleepWaitMenu"),
                RE::BSFixedString("TerminalMenu"),
                RE::BSFixedString("WorkshopMenu")
            };

            for (const auto& menuName : blockingMenuNames) {
                if (ui->GetMenuOpen(menuName)) {
                    return true;
                }
            }

            return false;
        }

        bool ShouldCaptureMenuInput()
        {
            return Hooks::IsMenuVisible() && !Hooks::IsModalDialogActive() && !IsBlockingGameMenuOpen();
        }

        bool ShouldRenderMenu()
        {
            return Hooks::IsMenuVisible() && !IsBlockingGameMenuOpen();
        }

        bool ShouldManageGameState()
        {
            return Hooks::IsMenuVisible() && !IsBlockingGameMenuOpen();
        }

        BOOL WINAPI HookedSetCursorPos(int x, int y)
        {
            if (ShouldCaptureMenuInput()) {
                return TRUE;
            }

            if (!originalSetCursorPos) {
                return FALSE;
            }

            return originalSetCursorPos(x, y);
        }

        BOOL WINAPI HookedClipCursor(const RECT* rect)
        {
            if (ShouldCaptureMenuInput()) {
                if (!originalClipCursor) {
                    return FALSE;
                }
                return originalClipCursor(nullptr);
            }

            if (!originalClipCursor) {
                return FALSE;
            }

            return originalClipCursor(rect);
        }

        void InstallCursorHooks()
        {
            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32) {
                return;
            }

            if (!originalSetCursorPos) {
                originalSetCursorPos = reinterpret_cast<SetCursorPos_t>(GetProcAddress(user32, "SetCursorPos"));
            }

            if (!originalClipCursor) {
                originalClipCursor = reinterpret_cast<ClipCursor_t>(GetProcAddress(user32, "ClipCursor"));
            }

            if (!originalSetCursorPos || !originalClipCursor) {
                return;
            }

            auto* dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(GetModuleHandle(nullptr));
            if (!dosHeader) {
                return;
            }

            auto* ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(dosHeader) + dosHeader->e_lfanew);
            if (!ntHeaders) {
                return;
            }

            auto* importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
                reinterpret_cast<BYTE*>(dosHeader) + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

            bool setCursorPosHooked = false;
            bool clipCursorHooked = false;

            while (importDesc && importDesc->Name && (!setCursorPosHooked || !clipCursorHooked)) {
                const char* moduleName = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(dosHeader) + importDesc->Name);
                if (_stricmp(moduleName, "user32.dll") == 0) {
                    auto* thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(dosHeader) + importDesc->FirstThunk);
                    while (thunk && thunk->u1.Function) {
                        DWORD oldProtect = 0;

                        if (!setCursorPosHooked && reinterpret_cast<void*>(thunk->u1.Function) == reinterpret_cast<void*>(originalSetCursorPos)) {
                            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_EXECUTE_READWRITE, &oldProtect);
                            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(&HookedSetCursorPos);
                            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &oldProtect);
                            setCursorPosHooked = true;
                        }

                        if (!clipCursorHooked && reinterpret_cast<void*>(thunk->u1.Function) == reinterpret_cast<void*>(originalClipCursor)) {
                            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), PAGE_EXECUTE_READWRITE, &oldProtect);
                            thunk->u1.Function = reinterpret_cast<ULONG_PTR>(&HookedClipCursor);
                            VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function), oldProtect, &oldProtect);
                            clipCursorHooked = true;
                        }

                        ++thunk;
                    }
                }

                ++importDesc;
            }
        }

        bool IsInputMessage(UINT msg)
        {
            switch (msg) {
            case WM_INPUT:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
            case WM_SYSCHAR:
                return true;
            default:
                return false;
            }
        }

        void MaintainFreeCursor(HWND hwnd)
        {
            if (!hwnd) {
                return;
            }

            RECT windowRect{};
            if (!GetWindowRect(hwnd, &windowRect)) {
                return;
            }

            POINT currentPos{};
            if (!GetCursorPos(&currentPos)) {
                return;
            }

            const LONG centerX = (windowRect.left + windowRect.right) / 2;
            const LONG centerY = (windowRect.top + windowRect.bottom) / 2;

            constexpr LONG centerThreshold = 2;
            const bool snappedToCenter =
                std::abs(currentPos.x - centerX) <= centerThreshold &&
                std::abs(currentPos.y - centerY) <= centerThreshold;

            if (snappedToCenter && freeCursorPosValid) {
                SetCursorPos(freeCursorPos.x, freeCursorPos.y);
                return;
            }

            freeCursorPos = currentPos;
            freeCursorPosValid = true;
        }
    }

    void Hooks::UpdateMenuInputState()
    {
        if (!ImGuiRenderer::IsInitialized()) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        const bool shouldCaptureInput = ShouldCaptureMenuInput();

        if (GamepadInput::IsUsingGamepad() && shouldCaptureInput) {
            io.MouseDrawCursor = false;
        } else {
            io.MouseDrawCursor = shouldCaptureInput;
        }

        if (shouldCaptureInput) {
            if (!cursorReleased) {
                ClipCursor(nullptr);
                ReleaseCapture();
                cursorReleased = true;
                freeCursorPosValid = false;
            }
            MaintainFreeCursor(gameWindow);
        } else if (cursorReleased) {
            cursorReleased = false;
            freeCursorPosValid = false;

            if (gameWindow) {
                RECT windowRect{};
                if (GetWindowRect(gameWindow, &windowRect) && originalClipCursor) {
                    originalClipCursor(&windowRect);
                }
            }
        }
    }

    void Hooks::Install()
    {
        if (originalPresent) {
            return;
        }

        menuVisible = Config::Get().showOnStartup;

        auto* rendererWindow = RE::BSGraphics::GetCurrentRendererWindow();
        if (!rendererWindow || !rendererWindow->swapChain) {
            REX::WARN("Renderer window not ready for Present hook");
            Logger::Warn("Renderer window not ready for Present hook");
            return;
        }

        gameWindow = reinterpret_cast<HWND>(rendererWindow->hwnd);
        if (gameWindow) {
            AttachWindowHook(gameWindow);
            InstallCursorHooks();
        }

        auto* swapChain = reinterpret_cast<IDXGISwapChain*>(rendererWindow->swapChain);
        auto** vtable = *reinterpret_cast<void***>(swapChain);
        originalPresent = reinterpret_cast<decltype(originalPresent)>(vtable[8]);

        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtable[8], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            REX::WARN("Failed to change vtable memory protection");
            Logger::Error("Failed to change vtable memory protection");
            originalPresent = nullptr;
            return;
        }

        vtable[8] = reinterpret_cast<void*>(&PresentHook);

        DWORD restoreProtect = 0;
        VirtualProtect(&vtable[8], sizeof(void*), oldProtect, &restoreProtect);

        REX::INFO("Present hook installed");
        Logger::Info("Present hook installed");
    }

    HRESULT __stdcall Hooks::PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        if (swapChain && !ImGuiRenderer::IsInitialized()) {
            DXGI_SWAP_CHAIN_DESC desc{};
            if (SUCCEEDED(swapChain->GetDesc(&desc))) {
                gameWindow = desc.OutputWindow;
                if (gameWindow) {
                    AttachWindowHook(gameWindow);
                    ImGuiRenderer::Initialize(swapChain, gameWindow);
                }
            }
        }

        if (ImGuiRenderer::IsInitialized()) {
            GamepadInput::Poll();

            if (GamepadInput::WasMenuTogglePressed()) {
                menuVisible = !menuVisible;
                Logger::Verbose(std::string("Menu toggled via controller: ") + (menuVisible ? "visible" : "hidden"));
            }

            UpdateMenuInputState();
            UpdateGamePause();
            UpdateHUDVisibility();

            if (ShouldRenderMenu()) {
                ImGuiRenderer::BeginFrame();
                MainWindow::Draw();
                ImGuiRenderer::EndFrame();
            }
        }

        if (originalPresent) {
            return originalPresent(swapChain, syncInterval, flags);
        }

        return S_OK;
    }

    bool Hooks::IsMenuVisible()
    {
        return menuVisible;
    }

    HWND Hooks::GetGameWindow()
    {
        return gameWindow;
    }

    void Hooks::SetModalDialogActive(bool active)
    {
        modalDialogActive = active;
    }

    bool Hooks::IsModalDialogActive()
    {
        return modalDialogActive;
    }

    void Hooks::UpdateGamePause()
    {
        auto* main = RE::Main::GetSingleton();
        if (!main) {
            return;
        }

        const bool shouldPause = Config::Get().pauseGameWhenMenuOpen && ShouldManageGameState();
        if (shouldPause) {
            if (!pauseStateManaged) {
                freezeTimeWasEnabledBeforeMenu = main->freezeTime;
                pauseStateManaged = true;
            }

            main->freezeTime = true;
            return;
        }

        if (!pauseStateManaged) {
            return;
        }

        main->freezeTime = freezeTimeWasEnabledBeforeMenu;
        freezeTimeWasEnabledBeforeMenu = false;
        pauseStateManaged = false;
    }

    void Hooks::UpdateHUDVisibility()
    {
        const bool shouldHideHUD = menuVisible && Config::Get().hidePlayerHUDWhenMenuOpen;
        auto* queue = RE::UIMessageQueue::GetSingleton();

        if (shouldHideHUD) {
            if (hudVisibilityManaged || !queue) {
                return;
            }

            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return;
            }

            hudWasVisibleBeforeHide = ui->GetMenuOpen(RE::HUDMenu::MENU_NAME);
            if (hudWasVisibleBeforeHide) {
                queue->AddMessage(RE::HUDMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide);
            }
            hudVisibilityManaged = true;
            return;
        }

        if (!hudVisibilityManaged || !queue) {
            return;
        }

        if (hudWasVisibleBeforeHide) {
            queue->AddMessage(RE::HUDMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow);
        }

        hudVisibilityManaged = false;
        hudWasVisibleBeforeHide = false;
    }

    LRESULT CALLBACK Hooks::WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_KEYUP) {
            const auto& settings = Config::Get();
            if (wParam == settings.toggleKey) {
                menuVisible = !menuVisible;
                Logger::Verbose(std::string("Menu visibility toggled: ") + (menuVisible ? "visible" : "hidden"));
                UpdateMenuInputState();
                UpdateGamePause();
                UpdateHUDVisibility();
                return 1;
            }
        }

        if ((msg == WM_ACTIVATEAPP || msg == WM_ACTIVATE || msg == WM_KILLFOCUS) && ShouldRenderMenu() && Config::Get().noPauseOnFocusLoss) {
            return 0;
        }

        if (modalDialogActive) {
            if (originalWndProc) {
                return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        if (ShouldCaptureMenuInput() && ImGuiRenderer::IsInitialized()) {
            if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
                return 1;
            }

            if (IsInputMessage(msg)) {
                return 1;
            }
        }

        if (originalWndProc) {
            return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void Hooks::AttachWindowHook(HWND hwnd)
    {
        if (!hwnd || originalWndProc) {
            return;
        }

        originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));
    }
}
