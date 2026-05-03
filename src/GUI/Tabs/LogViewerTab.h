#pragma once

#include <functional>

namespace ESPExplorerAE
{
    class LogViewerTab
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        static void Draw(const LocalizeFn& localize);
    };
}
