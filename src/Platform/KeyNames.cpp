#include "Platform/KeyNames.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <iterator>

namespace ESPExplorerAE
{
    std::string KeyboardKeyName(std::uint32_t key)
    {
        auto scanCode = MapVirtualKeyW(key, MAPVK_VK_TO_VSC_EX);
        // An E1 sequence cannot be represented by GetKeyNameText's E0 flag.
        if ((scanCode & 0xFF00u) == 0xE100u) scanCode = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
        if (!scanCode) return std::to_string(key);
        auto keyData = static_cast<LONG>((scanCode & 0xFFu) << 16);
        bool extended = (scanCode & 0xFF00u) == 0xE000u;
        // Layout mappings can choose a numpad alias even with VSC_EX. These
        // virtual keys identify the extended cluster described by Win32:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#extended-key-flag
        switch (key) {
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT: case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_NUMLOCK: case VK_DIVIDE: case VK_RCONTROL: case VK_RMENU:
            extended = true;
            break;
        default: break;
        }
        if (extended) keyData |= 1L << 24;
        wchar_t name[128]{};
        const auto length = GetKeyNameTextW(keyData, name, static_cast<int>(std::size(name)));
        if (length > 0) {
            const auto size = WideCharToMultiByte(CP_UTF8, 0, name, length, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string result(static_cast<std::size_t>(size), '\0');
                if (WideCharToMultiByte(CP_UTF8, 0, name, length, result.data(), size, nullptr, nullptr) == size) return result;
            }
        }
        return std::to_string(key);
    }
}
