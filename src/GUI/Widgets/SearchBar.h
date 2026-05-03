#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    class SearchBar
    {
    public:
        static bool Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value, bool* shouldFocus = nullptr);
    };
}
