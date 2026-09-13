#pragma once
#include "Core/RecordComparison.h"
#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    struct ComparisonViewState
    {
        std::optional<ComparisonRecord> a;
        std::optional<ComparisonRecord> b;
        bool open{};
        bool focusPending{};
        bool differencesOnly{};
    };
    void DrawComparisonWindow(ComparisonViewState& state, const BrowserView& view, BrowserRequests& requests);
}
