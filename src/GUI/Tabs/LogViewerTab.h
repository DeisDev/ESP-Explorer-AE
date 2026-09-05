#pragma once

#include "Core/Logs.h"
#include <functional>
#include <string_view>
#include <unordered_set>

namespace ESPExplorerAE
{
    struct LogViewerState
    {
        std::unordered_set<std::size_t> selectedLineIndexes;
        std::size_t lastClickedIndex{};
        std::uint64_t selectionRevision{};
        bool autoScroll{ true }, hasLastClicked{};
    };

    class LogViewerTab
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;
        static void Draw(LogViewerState& state, const LogSnapshot& view, LogRequests& requests, const LocalizeFn& localize);
    };
}
