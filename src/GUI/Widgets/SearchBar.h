#pragma once

#include <cstddef>
#include <string>

namespace ESPExplorerAE
{
    class SearchBar
    {
    public:
        // Call immediately after a text widget; Steam owns only that item's result.
        static bool ReadControllerText(const char* label, char* buffer, std::size_t bufferSize);
        static bool Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value, bool* shouldFocus = nullptr, const char* stableId = nullptr, const char* clearText = "X", float width = 0.0f);
    };
}
