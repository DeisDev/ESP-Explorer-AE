#pragma once

#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    struct NPCBrowserState
    {
        BrowserState browser;
        NPCQuery filters;
        char raceSearchBuffer[128]{};
        char factionSearchBuffer[128]{};
        bool raceDropdownJustOpened{};
        bool factionDropdownJustOpened{};
        BrowserQuery facetQuery;
        NPCFacets facets;
        std::optional<ResultRevision> facetRevision;

        void ResetSession()
        {
            browser.ResetSession();
            facetQuery.Clear();
            facetRevision.reset();
        }
    };

    class NPCBrowserTab
    {
    public:
        static void Draw(NPCBrowserState& state, const BrowserView& view, BrowserRequests& requests);
    };
}
