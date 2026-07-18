#pragma once

#include <cstddef>
#include <cstdint>

// Narrow Windows x64 declarations verified against Valve source-sdk-2013
// b8cfb12c0e083a2ef5b2f9f9b50f3902fa034474, src/public/steam/{
// steam_api_common.h, isteamutils.h, steam_api_flat.h }. No vtable slot dispatch.
namespace ESPExplorerAE::SteamKeyboardABI
{
    struct ISteamUtils;
    enum class InputMode : int { Normal = 0 };
    enum class LineMode : int { SingleLine = 0 };

    class CallbackBase
    {
    public:
        virtual void Run(void* data) = 0;
        virtual void Run(void* data, bool ioFailure, std::uint64_t call) = 0;
        virtual int GetCallbackSizeBytes() = 0;

    protected:
        // This exported ABI deliberately has no virtual destructor.
        std::uint8_t flags{ 0 };
        int callbackID{ 0 };
    };

    struct TextDismissed
    {
        static constexpr int ID = 714;
        bool submitted;
        std::uint32_t textLength;
        std::uint32_t appID;
    };

    using GetUtils = ISteamUtils* (__cdecl*)();
    using Show = bool (__cdecl*)(ISteamUtils*, InputMode, LineMode, const char*, std::uint32_t, const char*);
    using GetLength = std::uint32_t (__cdecl*)(ISteamUtils*);
    using GetText = bool (__cdecl*)(ISteamUtils*, char*, std::uint32_t);
    using IsEnabled = bool (__cdecl*)(ISteamUtils*);
    using GetAppID = std::uint32_t (__cdecl*)(ISteamUtils*);
    using Register = void (__cdecl*)(CallbackBase*, int);
    using Unregister = void (__cdecl*)(CallbackBase*);

    static_assert(sizeof(void*) == 8);
    static_assert(sizeof(CallbackBase) == 16);
    static_assert(sizeof(TextDismissed) == 12);
    static_assert(offsetof(TextDismissed, textLength) == 4);
    static_assert(offsetof(TextDismissed, appID) == 8);
}
