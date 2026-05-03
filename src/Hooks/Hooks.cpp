#include "Hooks/Hooks.h"

#include "Config/Config.h"
#include "GUI/ImGuiRenderer.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/FormActions.h"
#include "Input/GamepadInput.h"
#include "Logging/Logger.h"

#include <RE/B/BSGraphics.h>
#include <RE/C/ControlMap.h>
#include <RE/M/Main.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

#include <array>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ESPExplorerAE
{
    namespace
    {
        using ClipCursor_t = BOOL(WINAPI*)(const RECT*);
        ClipCursor_t originalClipCursor{ nullptr };

        std::array<bool, 256> trackedKeys{};
        std::array<bool, 5> trackedMouseButtons{};

        bool IsBlockingGameMenuOpen()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return false;
            }

            static const std::array<RE::BSFixedString, 11> blockingMenuNames{
                RE::BSFixedString("BarterMenu"),
                RE::BSFixedString("ContainerMenu"),
                RE::BSFixedString("DialogueMenu"),
                RE::BSFixedString("LevelUpMenu"),
                RE::BSFixedString("LockpickingMenu"),
                RE::BSFixedString("LooksMenu"),
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
            return Hooks::IsMenuVisible() && !IsBlockingGameMenuOpen() && Hooks::HasGameWindowFocus();
        }

        BOOL WINAPI HookedClipCursor(const RECT* rect)
        {
            if (ShouldCaptureMenuInput()) {
                return originalClipCursor(nullptr);
            }

            return originalClipCursor(rect);
        }

        void InstallCursorHooks()
        {
            HMODULE user32 = GetModuleHandleA("user32.dll");
            if (!user32) {
                return;
            }

            if (!originalClipCursor) {
                originalClipCursor = reinterpret_cast<ClipCursor_t>(GetProcAddress(user32, "ClipCursor"));
            }

            if (!originalClipCursor) {
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

            bool clipCursorHooked = false;

            while (importDesc && importDesc->Name && !clipCursorHooked) {
                const char* moduleName = reinterpret_cast<const char*>(reinterpret_cast<BYTE*>(dosHeader) + importDesc->Name);
                if (_stricmp(moduleName, "user32.dll") == 0) {
                    auto* thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(dosHeader) + importDesc->FirstThunk);
                    while (thunk && thunk->u1.Function) {
                        DWORD oldProtect = 0;

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

        bool IsMouseMessage(UINT msg)
        {
            switch (msg) {
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
                return true;
            default:
                return false;
            }
        }

        bool IsKeyboardMessage(UINT msg)
        {
            switch (msg) {
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

        void UpdateTrackedKeyboardState(UINT msg, WPARAM wParam)
        {
            if (wParam >= trackedKeys.size()) {
                return;
            }

            switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                trackedKeys[static_cast<std::size_t>(wParam)] = true;
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                trackedKeys[static_cast<std::size_t>(wParam)] = false;
                break;
            default:
                break;
            }
        }

        void UpdateTrackedMouseState(UINT msg, WPARAM wParam)
        {
            switch (msg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
                trackedMouseButtons[0] = true;
                break;
            case WM_LBUTTONUP:
                trackedMouseButtons[0] = false;
                break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONDBLCLK:
                trackedMouseButtons[1] = true;
                break;
            case WM_RBUTTONUP:
                trackedMouseButtons[1] = false;
                break;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONDBLCLK:
                trackedMouseButtons[2] = true;
                break;
            case WM_MBUTTONUP:
                trackedMouseButtons[2] = false;
                break;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONDBLCLK:
                if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
                    trackedMouseButtons[3] = true;
                } else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) {
                    trackedMouseButtons[4] = true;
                }
                break;
            case WM_XBUTTONUP:
                if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
                    trackedMouseButtons[3] = false;
                } else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) {
                    trackedMouseButtons[4] = false;
                }
                break;
            default:
                break;
            }
        }

        void ReleaseTrackedInputs()
        {
            std::array<INPUT, 261> releaseInputs{};
            std::size_t releaseCount = 0;

            for (std::size_t vk = 0; vk < trackedKeys.size(); ++vk) {
                if (!trackedKeys[vk]) {
                    continue;
                }

                INPUT input{};
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = static_cast<WORD>(vk);
                input.ki.dwFlags = KEYEVENTF_KEYUP;
                releaseInputs[releaseCount++] = input;
                trackedKeys[vk] = false;
            }

            constexpr std::array<DWORD, 5> mouseReleaseFlags{
                MOUSEEVENTF_LEFTUP,
                MOUSEEVENTF_RIGHTUP,
                MOUSEEVENTF_MIDDLEUP,
                MOUSEEVENTF_XUP,
                MOUSEEVENTF_XUP
            };
            constexpr std::array<DWORD, 5> mouseReleaseData{
                0,
                0,
                0,
                XBUTTON1,
                XBUTTON2
            };

            for (std::size_t i = 0; i < trackedMouseButtons.size(); ++i) {
                if (!trackedMouseButtons[i]) {
                    continue;
                }

                INPUT input{};
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = mouseReleaseFlags[i];
                input.mi.mouseData = mouseReleaseData[i];
                releaseInputs[releaseCount++] = input;
                trackedMouseButtons[i] = false;
            }

            if (releaseCount > 0) {
                SendInput(static_cast<UINT>(releaseCount), releaseInputs.data(), sizeof(INPUT));
            }
        }

        void ResetTrackedInputs()
        {
            trackedKeys.fill(false);
            trackedMouseButtons.fill(false);
        }

        bool IsTabKeyMessage(UINT msg, WPARAM wParam)
        {
            if (wParam != VK_TAB) {
                return false;
            }

            switch (msg) {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return true;
            default:
                return false;
            }
        }
    }

    void Hooks::UpdateCursorState()
    {
        const bool shouldCapture = ShouldCaptureMenuInput();

        auto* controlMap = RE::ControlMap::GetSingleton();
        if (controlMap) {
            if (shouldCapture && !ignoreInputManaged) {
                controlMap->ignoreKeyboardMouse = true;
                ignoreInputManaged = true;
            } else if (!shouldCapture && ignoreInputManaged) {
                controlMap->ignoreKeyboardMouse = false;
                ignoreInputManaged = false;
            }
        }

        const bool wantCursor = shouldCapture && !GamepadInput::IsUsingGamepad();
        if (wantCursor && !cursorShowing) {
            ::ShowCursor(TRUE);
            cursorShowing = true;
        } else if (!wantCursor && cursorShowing) {
            ::ShowCursor(FALSE);
            cursorShowing = false;
        }

        if (shouldCapture && originalClipCursor) {
            originalClipCursor(nullptr);
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
                Logger::Verbose("Menu toggle requested via gamepad");
                SetMenuVisible(!menuVisible);
            }

            UpdateCursorState();
            UpdateGamePause();
            UpdateMenuGodMode();
            UpdateHUDVisibility();

            if (ShouldRenderMenu()) {
                ImGuiRenderer::BeginFrame();
                MainWindow::Draw();
                ImGuiRenderer::EndFrame();
            }
        }

        HRESULT result = S_OK;
        if (originalPresent) {
            result = originalPresent(swapChain, syncInterval, flags);
        }

        return result;
    }

    bool Hooks::IsMenuVisible()
    {
        return menuVisible;
    }

    void Hooks::SetMenuVisible(bool visible)
    {
        if (menuVisible == visible) {
            return;
        }

        menuVisible = visible;
        if (menuVisible) {
            ReleaseTrackedInputs();
            MainWindow::HandleMenuVisibilityChanged(true);
        } else {
            MainWindow::HandleMenuVisibilityChanged(false);
            ResetTrackedInputs();
        }

        Logger::Verbose(std::string("Menu visibility changed: ") + (menuVisible ? "visible" : "hidden"));
        if (menuVisible && IsBlockingGameMenuOpen()) {
            Logger::Verbose("Menu rendering suppressed because a blocking game menu is open");
        }
        UpdateCursorState();
        UpdateGamePause();
        UpdateMenuGodMode();
        UpdateHUDVisibility();
    }

    bool Hooks::HasGameWindowFocus()
    {
        return gameWindowHasFocus;
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

            static const RE::BSFixedString powerArmorHUDMenuName("PowerArmorHUDMenu");
            if (ui->GetMenuOpen(powerArmorHUDMenuName)) {
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

    void Hooks::UpdateMenuGodMode()
    {
        const bool shouldEnableGodMode = menuVisible && !IsBlockingGameMenuOpen() && Config::Get().godModeWhenMenuOpen;

        if (shouldEnableGodMode) {
            if (!godModeStateManaged) {
                godModeWasEnabledBeforeMenu = FormActions::IsPlayerGodModeEnabled();
                godModeStateManaged = true;
            }

            if (!FormActions::IsPlayerGodModeEnabled()) {
                FormActions::SetPlayerGodModeEnabled(true);
            }
            return;
        }

        if (!godModeStateManaged) {
            return;
        }

        if (!godModeWasEnabledBeforeMenu) {
            FormActions::SetPlayerGodModeEnabled(false);
        }

        godModeWasEnabledBeforeMenu = false;
        godModeStateManaged = false;
    }

    LRESULT CALLBACK Hooks::WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        UpdateTrackedKeyboardState(msg, wParam);
        UpdateTrackedMouseState(msg, wParam);

        if (msg == WM_ACTIVATEAPP) {
            gameWindowHasFocus = wParam != 0;
            UpdateGamePause();
        } else if (msg == WM_ACTIVATE) {
            gameWindowHasFocus = LOWORD(wParam) != WA_INACTIVE;
            UpdateGamePause();
        } else if (msg == WM_SETFOCUS) {
            gameWindowHasFocus = true;
            UpdateGamePause();
        } else if (msg == WM_KILLFOCUS) {
            gameWindowHasFocus = false;
            UpdateGamePause();
            ResetTrackedInputs();
        }

        if (msg == WM_KEYUP) {
            const auto& settings = Config::Get();
            if (wParam == settings.toggleKey) {
                Logger::Verbose("Menu toggle requested via keyboard");
                SetMenuVisible(!menuVisible);
                return 1;
            }
        }

        if (modalDialogActive) {
            if (originalWndProc) {
                return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        if (ShouldCaptureMenuInput() && ImGuiRenderer::IsInitialized()) {
            if (IsMouseMessage(msg) && originalClipCursor) {
                originalClipCursor(nullptr);
            }

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

        SetLastError(0);
        const auto previousWndProc = SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook));
        if (previousWndProc == 0) {
            const DWORD error = GetLastError();
            if (error != 0) {
                Logger::Error("Failed to attach window procedure hook");
                return;
            }
        }

        originalWndProc = reinterpret_cast<WNDPROC>(previousWndProc);
        Logger::Info("Window procedure hook installed");
    }
}
