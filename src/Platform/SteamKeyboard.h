#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace ESPExplorerAE
{
    class SteamKeyboard
    {
    public:
        static bool Show(std::uint32_t owner, const char* description, const char* existingText, std::size_t capacity);
        static void Abandon();
        static bool IsOpen();
        static bool IsWaiting();
        static std::optional<std::string> Take(std::uint32_t owner);
        // Call only during controlled shutdown, never from DllMain.
        static void Shutdown();
    };
}
