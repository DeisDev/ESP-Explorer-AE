#pragma once

#include "GUI/BrowserState.h"
#include "GUI/Widgets/FormDetailsView.h"
#include "Core/WorkspaceNavigation.h"

namespace ESPExplorerAE
{
    struct RecordInspectorState
    {
        InspectionHistory history;
        bool restoreScroll{ true };
        bool open{ true };
        bool focusPending{};
        std::uint64_t windowID{};
        bool canPin{ true };
        std::unordered_map<std::uint32_t, int> quantities;
    };

    struct RecordInspectorRequests
    {
        BrowserRequests records;
        std::optional<std::uint32_t> open;
        std::vector<std::uint32_t> pins;
        int historyMove{};
    };

    void DrawRecordInspector(RecordInspectorState& state, const BrowserView& view,
        std::shared_ptr<const RecordDetails> details, bool advanced, bool outsideScope, RecordInspectorRequests& requests);
}
