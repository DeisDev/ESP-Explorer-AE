#include "Platform/SteamKeyboard.h"
#include "Platform/SteamKeyboardABI.h"
#include "Core/TextInputSession.h"

#include <Windows.h>
#include <algorithm>
#include <mutex>
#include <vector>

namespace ESPExplorerAE
{
    namespace
    {
        namespace ABI = SteamKeyboardABI;
        struct Adapter : ABI::CallbackBase
        {
            ABI::ISteamUtils* utils{};
            ABI::Show show{};
            ABI::GetLength length{};
            ABI::GetText text{};
            ABI::IsEnabled enabled{};
            ABI::GetAppID appID{};
            ABI::Register registerCallback{};
            ABI::Unregister unregisterCallback{};
            bool registered{};
            bool stopped{};
            TextInputSession input;
            std::mutex mutex;

            bool Load()
            {
                if (stopped) return false; // Setup and shutdown share the render owner.
                if (registered) return true;
                const auto module = GetModuleHandleW(L"steam_api64.dll");
                if (!module) return false;
                const auto getUtils = reinterpret_cast<ABI::GetUtils>(GetProcAddress(module, "SteamAPI_SteamUtils_v010"));
                show = reinterpret_cast<ABI::Show>(GetProcAddress(module, "SteamAPI_ISteamUtils_ShowGamepadTextInput"));
                length = reinterpret_cast<ABI::GetLength>(GetProcAddress(module, "SteamAPI_ISteamUtils_GetEnteredGamepadTextLength"));
                text = reinterpret_cast<ABI::GetText>(GetProcAddress(module, "SteamAPI_ISteamUtils_GetEnteredGamepadTextInput"));
                enabled = reinterpret_cast<ABI::IsEnabled>(GetProcAddress(module, "SteamAPI_ISteamUtils_IsOverlayEnabled"));
                appID = reinterpret_cast<ABI::GetAppID>(GetProcAddress(module, "SteamAPI_ISteamUtils_GetAppID"));
                registerCallback = reinterpret_cast<ABI::Register>(GetProcAddress(module, "SteamAPI_RegisterCallback"));
                unregisterCallback = reinterpret_cast<ABI::Unregister>(GetProcAddress(module, "SteamAPI_UnregisterCallback"));
                if (!getUtils || !show || !length || !text || !enabled || !appID || !registerCallback || !unregisterCallback) return false;
                utils = getUtils();
                if (!utils) return false;
                callbackID = ABI::TextDismissed::ID;
                registerCallback(this, callbackID);
                registered = true;
                return true;
            }

            void Run(void* data) override
            {
                if (!data) return;
                const auto& result = *static_cast<const ABI::TextDismissed*>(data);
                std::lock_guard lock(mutex);
                if (stopped || !utils || result.appID != appID(utils)) return;
                try {
                    input.Dismiss(result.submitted, [&]() -> std::optional<std::string> {
                        const auto count = length(utils);
                        if (count > 4096) return std::nullopt;
                        if (count == 0) return std::string{};
                        std::vector<char> buffer(static_cast<std::size_t>(count) + 1, '\0');
                        if (!text(utils, buffer.data(), count)) return std::nullopt;
                        return std::string(buffer.data());
                    });
                } catch (...) {
                    input.Unavailable();
                }
            }

            void Run(void* data, bool failed, std::uint64_t) override
            {
                if (!failed) Run(data);
            }
            int GetCallbackSizeBytes() override { return sizeof(ABI::TextDismissed); }
        };

        // No destructor calls into Steam during DLL teardown. The host owns
        // Steam initialization, callback pumping and shutdown for its lifetime.
        Adapter adapter;
    }

    bool SteamKeyboard::Show(std::uint32_t owner, const char* description, const char* existingText, std::size_t capacity)
    {
        if (!description || !existingText || capacity < 2) return false;
        // Setup/show are render-thread operations; callbacks may arrive on the
        // host callback thread, and only the detached result crosses the mutex.
        if (!adapter.Load() || !adapter.enabled(adapter.utils)) return false;
        {
            std::lock_guard lock(adapter.mutex);
            if (!adapter.input.Begin(owner)) return false;
        }
        const auto maxChars = static_cast<std::uint32_t>((std::min)(capacity - 1, std::size_t{ 256 }));
        if (adapter.show(adapter.utils, ABI::InputMode::Normal, ABI::LineMode::SingleLine, description, maxChars, existingText)) return true;
        std::lock_guard lock(adapter.mutex);
        adapter.input.Unavailable();
        return false;
    }

    void SteamKeyboard::Abandon()
    {
        std::lock_guard lock(adapter.mutex);
        adapter.input.Abandon();
    }

    bool SteamKeyboard::IsWaiting()
    {
        std::lock_guard lock(adapter.mutex);
        return adapter.input.IsWaiting();
    }

    bool SteamKeyboard::IsOpen()
    {
        std::lock_guard lock(adapter.mutex);
        return adapter.input.IsOpen();
    }

    std::optional<std::string> SteamKeyboard::Take(std::uint32_t owner)
    {
        std::lock_guard lock(adapter.mutex);
        return adapter.input.Take(owner);
    }

    void SteamKeyboard::Shutdown()
    {
        {
            // Wait for any callback already reading host data, then make late
            // callbacks inert before unregistering outside the callback mutex.
            std::lock_guard lock(adapter.mutex);
            adapter.stopped = true;
            adapter.input.Unavailable();
        }
        if (adapter.registered) {
            adapter.unregisterCallback(&adapter);
            adapter.registered = false;
        }
    }
}
