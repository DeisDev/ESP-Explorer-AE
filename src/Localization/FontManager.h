#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    class FontManager
    {
    public:
        static bool Build(float fontSize, std::string_view languageCode);

    private:
        static std::filesystem::path ResolveFontsDirectory();
    };
}
