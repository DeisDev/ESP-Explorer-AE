#pragma once

#include <imgui.h>

#include <string_view>

namespace ESPExplorerAE
{
    class SharedUtils
    {
    public:
        static bool ContainsCaseInsensitive(std::string_view text, std::string_view query);
        static bool EqualsCaseInsensitive(std::string_view left, std::string_view right);
        static bool ContainsByMode(std::string_view text, std::string_view query, bool caseSensitive);
        static void DrawCurrentItemChrome(bool active, bool hovered, bool accentTop, bool accentLeft);
        static void DrawSectionLabel(const char* label);
    };
}
