#include "Localization/Language.h"
#include "Hooks/Hooks.h"

#include "Config/Config.h"
#include "App/ActionService.h"
#include "App/CatalogService.h"
#include "App/Lifecycle.h"
#include "App/Profiler.h"
#include "Core/Profiling.h"
#include "App/SettingsService.h"
#include "App/OverlayController.h"
#include "Core/ScopeExit.h"
#include "Core/ToggleHint.h"
#include "Platform/SteamKeyboard.h"
#include "Platform/HookTransaction.h"
#include "GUI/ImGuiRenderer.h"
#include "GUI/MainWindow.h"
#include "GUI/Widgets/ToggleHintView.h"
#include "Input/GamepadInput.h"

#include <RE/B/BSGraphics.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

#include <array>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <chrono>

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
        std::mutex installationMutex;
        std::mutex renderMutex;
        HookTransaction hookTransaction;
        IDXGISwapChain* hookedSwapChain{};
        DWORD renderThread{};
        ToggleHint toggleHint;
        std::atomic<bool> hooksReady{};
        std::atomic<ClipCursor_t> originalClipCursor{};

        std::mutex inputMutex;
        std::deque<MSG> inputMessages;
        bool inputResetRequested{};
        std::atomic<std::uint32_t> toggleKey{ VK_INSERT };
        constexpr ULONG_PTR kReleaseInputTag = 0x4553504145494E50;
        std::array<bool, 256> trackedKeys{};
        std::array<bool, 5> trackedMouseButtons{};

        bool ShouldCaptureMenuInput() { return !Lifecycle::Shutdown().Requested() && OverlayController::Decision().capture; }

        BOOL WINAPI HookedClipCursor(const RECT* rect)
        {
            ClipCursor_t previous;
            {
                std::lock_guard lock(installationMutex);
                previous = originalClipCursor;
            }
            return previous ? previous(hooksReady && !Lifecycle::Shutdown().Requested() && ShouldCaptureMenuInput() ? nullptr : rect) : FALSE;
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
            {
            std::lock_guard lock(inputMutex);

            for (std::size_t vk = 0; vk < trackedKeys.size(); ++vk) {
                if (!trackedKeys[vk]) {
                    continue;
                }

                INPUT input{};
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = static_cast<WORD>(vk);
                input.ki.dwFlags = KEYEVENTF_KEYUP;
                input.ki.dwExtraInfo = kReleaseInputTag;
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
                input.mi.dwExtraInfo = kReleaseInputTag;
                releaseInputs[releaseCount++] = input;
                trackedMouseButtons[i] = false;
            }

            }
            if (releaseCount > 0) {
                SendInput(static_cast<UINT>(releaseCount), releaseInputs.data(), sizeof(INPUT));
            }
        }

        void ResetTrackedInputs()
        {
            std::lock_guard lock(inputMutex);
            trackedKeys.fill(false);
            trackedMouseButtons.fill(false);
        }

        void UpdateWindowFocus(bool focused)
        {
            std::lock_guard lock(inputMutex);
            OverlayController::SetFocused(focused);
            if (focused) return;
            trackedKeys.fill(false);
            trackedMouseButtons.fill(false);
            inputMessages.clear();
            PerformanceProfile().Observe(ProfileGauge::InputQueue, 0);
            inputResetRequested = true;
        }

        void RefreshWindowFocus(HWND window)
        {
            // Activation messages can be missed during startup or intercepted by
            // another window hook. Reconcile with the actual foreground window;
            // owned dialogs and other applications must still suspend capture.
            UpdateWindowFocus(window && GetForegroundWindow() == window && !IsIconic(window));
        }

    }

    void Hooks::UpdateCursorState()
    {
        const bool shouldCapture = ShouldCaptureMenuInput();

        const bool wantCursor = shouldCapture && !GamepadInput::IsUsingGamepad();
        if (wantCursor && !cursorShowing) {
            ::ShowCursor(TRUE);
            cursorShowing = true;
        } else if (!wantCursor && cursorShowing) {
            ::ShowCursor(FALSE);
            cursorShowing = false;
        }

        if (shouldCapture && originalClipCursor) {
            originalClipCursor.load()(nullptr);
        }
    }

    void Hooks::Install()
    {
        std::lock_guard lock(installationMutex);
        if (Lifecycle::Shutdown().Requested() || hookTransaction.Ready()) return;
        auto* rendererWindow = RE::BSGraphics::GetCurrentRendererWindow();
        if (!rendererWindow || !rendererWindow->swapChain || !rendererWindow->hwnd) {
            REX::DEBUG("Renderer window not ready for hooks");
            return;
        }
        const auto window = reinterpret_cast<HWND>(rendererWindow->hwnd);
        auto* swapChain = reinterpret_cast<IDXGISwapChain*>(rendererWindow->swapChain);
        auto** vtable = *reinterpret_cast<void***>(swapChain);
        auto** cursorSlot = FindImport(GetModuleHandleW(nullptr), "user32.dll", "ClipCursor");
        const bool installed = hookTransaction.Install(window, WndProcHook, &vtable[8],
            reinterpret_cast<void*>(&PresentHook), cursorSlot, reinterpret_cast<void*>(&HookedClipCursor));
        // Publish all chain targets before any hooked callback passes the gate.
        originalPresent = reinterpret_cast<PresentFunction>(hookTransaction.PreviousPresent());
        originalWndProc = hookTransaction.PreviousWindow();
        originalClipCursor = reinterpret_cast<ClipCursor_t>(hookTransaction.PreviousCursor());
        hooksReady = installed;
        if (!installed) {
            REX::WARN("Hook installation failed; rollback attempted and pending restoration will be retried");
            return;
        }
        gameWindow = window;
        hookedSwapChain = swapChain;
        OverlayController::SetVisible(Config::Get().showOnStartup);
        RefreshWindowFocus(window);
        REX::INFO("Hooks installed: install thread {}, window thread {}, optional cursor import {}",
            GetCurrentThreadId(), GetWindowThreadProcessId(window, nullptr), cursorSlot != nullptr);
    }

    HRESULT __stdcall Hooks::PresentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        PresentFunction previous;
        bool active;
        {
            std::lock_guard lock(installationMutex);
            previous = originalPresent;
            active = hooksReady && swapChain == hookedSwapChain;
        }
        const auto forward = [&] { return previous ? previous(swapChain, syncInterval, flags) : S_OK; };
        static thread_local bool rendering{};
        if (!active || rendering || (flags & DXGI_PRESENT_TEST)) return forward();
        std::unique_lock renderLock(renderMutex, std::try_to_lock);
        if (!renderLock.owns_lock()) return forward();
        // The first target Present owns all ImGui and UI state. Another Present
        // thread/swap chain always forwards without entering that state.
        if (!renderThread) {
            renderThread = GetCurrentThreadId();
            REX::INFO("Overlay render context: thread {}", renderThread);
        }
        if (renderThread != GetCurrentThreadId()) return forward();
        rendering = true;
        ScopeExit clearRendering([] { rendering = false; });
        auto* previousContext = ImGui::GetCurrentContext();
        ScopeExit restoreContext([&] { ImGui::SetCurrentContext(previousContext); });
        static std::chrono::steady_clock::time_point retryRenderer{};
        try {
            auto& shutdown = Lifecycle::Shutdown();
            if (shutdown.Requested()) {
                shutdown.PumpRender([&] {
                    OverlayController::SetVisible(false);
                    OverlayController::SetRendererState(false, false);
                    GamepadInput::CloseSteamKeyboard();
                    SteamKeyboard::Shutdown();
                    MainWindow::Shutdown();
                    ResetTrackedInputs();
                    {
                        std::lock_guard lock(inputMutex);
                        inputMessages.clear();
                        PerformanceProfile().Observe(ProfileGauge::InputQueue, 0);
                        inputResetRequested = false;
                    }
                    UpdateCursorState();
                    if (previousContext == ImGuiRenderer::GetContext()) previousContext = nullptr;
                    ImGuiRenderer::Shutdown(); // Attempts the pending save immediately, once.
                    Profiler::Stop();
                }, [] {
                    Config::FlushPendingSaveIfDue(); // Failed writes retain their normal retry schedule.
                    return !Config::HasPendingSave();
                });
                ImGui::SetCurrentContext(previousContext);
                restoreContext.Release();
                return forward();
            }
            Profiler::Configure(Config::Get().profilePerformance);
            {
                const ProfileScope profileScope(IsMenuVisible() ? ProfileMetric::OverlayFrame : ProfileMetric::OverlayHiddenFrame);
                const Language::Frame languageFrame;
                SettingsService::PumpRender();
                Config::FlushPendingSaveIfDue();
                const auto& settings = Config::Get();
                toggleKey = settings.toggleKey;
                RefreshWindowFocus(gameWindow);
                ActionService::UpdatePolicy(settings.componentSubstitution, settings.allowGameplayActionsInMainMenu);
                OverlayController::Configure({ settings.pauseGameWhenMenuOpen, settings.hidePlayerHUDWhenMenuOpen, settings.godModeWhenMenuOpen });
                if (swapChain && !ImGuiRenderer::IsInitialized() && std::chrono::steady_clock::now() >= retryRenderer) {
                    retryRenderer = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                    DXGI_SWAP_CHAIN_DESC desc{};
                    if (SUCCEEDED(swapChain->GetDesc(&desc))) {
                        if (desc.OutputWindow == gameWindow.load()) ImGuiRenderer::Initialize(swapChain, gameWindow);
                    }
                }
                const bool ready = ImGuiRenderer::IsInitialized();
                OverlayController::SetRendererState(ready, SteamKeyboard::IsWaiting());
                if (ready) {
                    ImGui::SetCurrentContext(ImGuiRenderer::GetContext());
                    const auto facts = OverlayController::Facts();
                    GamepadInput::Poll(facts.focused && !facts.modal && !facts.keyboardDialog);
                    if (!shutdown.Requested() && GamepadInput::WasMenuTogglePressed()) OverlayController::Toggle();

                    static bool lastVisible{};
                    const bool visible = IsMenuVisible();
                    if (visible != lastVisible) {
                        lastVisible = visible;
                        MainWindow::HandleMenuVisibilityChanged(visible);
                        if (visible) ReleaseTrackedInputs();
                        else {
                            GamepadInput::CloseSteamKeyboard();
                            ResetTrackedInputs();
                            Config::FlushPendingSave();
                        }
                    }
                    std::deque<MSG> messages;
                    bool reset;
                    {
                        std::lock_guard lock(inputMutex);
                        messages.swap(inputMessages);
                        PerformanceProfile().Observe(ProfileGauge::InputQueue, inputMessages.size());
                        reset = inputResetRequested;
                        inputResetRequested = false;
                    }
                    const auto decision = shutdown.Requested() ? OverlayDecision{} : OverlayController::Decision();
                    if (reset || !decision.capture) {
                        ImGui::GetIO().ClearInputKeys();
                        ImGui::GetIO().ClearEventsQueue();
                    }
                    if (decision.capture) {
                        for (const auto& message : messages) {
                            ImGui_ImplWin32_WndProcHandler(message.hwnd, message.message, message.wParam, message.lParam);
                        }
                    }
                    UpdateCursorState();
                    const auto hintOpacity = shutdown.Requested() ? 0.0f : toggleHint.Update(
                        OverlayController::Facts(), CatalogService::Read()->ready, std::chrono::steady_clock::now());
                    if (decision.render || hintOpacity > 0.0f) {
                        ImGuiRenderer::BeginFrame();
                        if (decision.render) MainWindow::Draw();
                        if (hintOpacity > 0.0f) {
                            const auto localize = [](std::string_view section, std::string_view key, const char* fallback) {
                                const auto text = Language::FrameText(section, key);
                                return text.empty() ? fallback : text.data();
                            };
                            const auto key = GamepadInput::IsUsingGamepad() ? std::string(localize("Settings", "sToggleCombo", "RB + X")) : SettingsService::ToggleKeyName();
                            ToggleHintView::Draw(key, hintOpacity, localize);
                        }
                        ImGuiRenderer::EndFrame();
                    }
                    Profiler::SampleImGui();
                }
            }
            Profiler::Pump(); // Report I/O is excluded from the plugin frame sample.
        } catch (const std::exception& error) {
            REX::WARN("Overlay frame failed: {}", error.what());
            OverlayController::SetVisible(false);
            OverlayController::SetRendererState(false, SteamKeyboard::IsWaiting());
            if (previousContext == ImGuiRenderer::GetContext()) previousContext = nullptr;
            ImGuiRenderer::Shutdown();
            UpdateCursorState();
        } catch (...) {
            REX::WARN("Overlay frame failed with an unknown exception");
            OverlayController::SetVisible(false);
            OverlayController::SetRendererState(false, SteamKeyboard::IsWaiting());
            if (previousContext == ImGuiRenderer::GetContext()) previousContext = nullptr;
            ImGuiRenderer::Shutdown();
            UpdateCursorState();
        }
        ImGui::SetCurrentContext(previousContext);
        restoreContext.Release();
        return forward();
    }

    bool Hooks::RestoreForShutdown()
    {
        std::lock_guard lock(installationMutex);
        hooksReady = false;
        // Previous targets stay published: a later hook can still chain through
        // these callbacks, and F4SE's permanent task cannot be unregistered.
        return hookTransaction.Restore();
    }

    bool Hooks::IsMenuVisible() { return OverlayController::Facts().visible; }
    void Hooks::SetMenuVisible(bool visible) { if (!visible || !Lifecycle::Shutdown().Requested()) OverlayController::SetVisible(visible); }
    bool Hooks::HasGameWindowFocus() { return OverlayController::Facts().focused; }
    HWND Hooks::GetGameWindow() { return gameWindow; }
    void Hooks::SetModalDialogActive(bool active) { OverlayController::SetModal(active); }
    bool Hooks::IsModalDialogActive() { return OverlayController::Facts().modal; }

    LRESULT CALLBACK Hooks::WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    try {
        WNDPROC previous;
        bool active;
        {
            std::lock_guard lock(installationMutex);
            previous = originalWndProc;
            active = hooksReady && !Lifecycle::Shutdown().Requested() && hwnd == gameWindow.load();
        }
        const auto forward = [&] { return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam); };
        if (!active) return forward();
        // These releases belong to inputs the game saw before capture started.
        // Let them reach the prior procedure, without toggling or re-queueing.
        if (IsInputMessage(msg) && static_cast<ULONG_PTR>(GetMessageExtraInfo()) == kReleaseInputTag) return forward();
        {
            std::lock_guard lock(inputMutex);
            UpdateTrackedKeyboardState(msg, wParam);
            UpdateTrackedMouseState(msg, wParam);
        }
        if (msg == WM_ACTIVATEAPP) UpdateWindowFocus(wParam != 0);
        else if (msg == WM_ACTIVATE) UpdateWindowFocus(LOWORD(wParam) != WA_INACTIVE);
        else if (msg == WM_SETFOCUS) UpdateWindowFocus(true);
        else if (msg == WM_KILLFOCUS) UpdateWindowFocus(false);
        else if (IsInputMessage(msg)) RefreshWindowFocus(hwnd);
        const auto facts = OverlayController::Facts();
        if (msg == WM_KEYUP && wParam == toggleKey && facts.focused && !facts.modal && !facts.keyboardDialog &&
            static_cast<ULONG_PTR>(GetMessageExtraInfo()) != kReleaseInputTag) {
            OverlayController::Toggle();
            return 1;
        }
        if (msg == WM_CLOSE || msg == WM_DESTROY || msg == WM_ENDSESSION) {
            OverlayController::SetVisible(false);
            Config::FlushPendingSave();
        }
        if (ShouldCaptureMenuInput() && IsInputMessage(msg)) {
            // WM_INPUT contains a transient OS handle; ImGui uses the detached
            // Win32 mouse/key messages, so never retain that handle for later.
            if (msg != WM_INPUT) {
                std::lock_guard lock(inputMutex);
                if (inputMessages.size() >= 512) {
                    inputMessages.clear();
                    inputResetRequested = true;
                }
                if (msg == WM_MOUSEMOVE && !inputMessages.empty() && inputMessages.back().message == WM_MOUSEMOVE) {
                    inputMessages.back() = MSG{ hwnd, msg, wParam, lParam };
                } else {
                    inputMessages.push_back(MSG{ hwnd, msg, wParam, lParam });
                }
                PerformanceProfile().Observe(ProfileGauge::InputQueue, inputMessages.size());
            } else {
                // DefWindowProc performs required raw-input cleanup.
                DefWindowProc(hwnd, msg, wParam, lParam);
            }
            return 1;
        }
        return forward();
    }
    catch (...) {
        OverlayController::SetVisible(false);
        WNDPROC previous;
        { std::lock_guard lock(installationMutex); previous = originalWndProc; }
        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

}
