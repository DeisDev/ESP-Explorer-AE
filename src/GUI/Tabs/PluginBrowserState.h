#pragma once

#include "GUI/Widgets/RecordFiltersWidget.h"

#include "Core/PluginQuery.h"
#include "Core/Actions.h"
#include "Core/TreeSelection.h"
#include <array>
#include <deque>

namespace ESPExplorerAE
{
    struct PluginBrowserState
    {
        AdvancedFilterEditorState filterEditor;
        std::string search;
        bool structuredSearch{};
        RecordScope scope;
        std::array<char, 1025> searchBuffer{};
        std::array<char, 257> archiveSearchBuffer{};
        std::string archiveSearch;
        std::string diagnosticsPlugin;
        bool focusPending{};
        bool showUnknown{};
        bool allRuntimeRecords{};
        bool globalSearch{};
        bool collapseDiagnostics{};
        int spawnQuantity{ 1 };
        std::uint32_t detailsFormID{};
        std::string detailsPlugin;
        ActionAdmission admission{ ActionAdmission::Accepted };
        std::unordered_map<std::uint32_t, int> contextQuantities;
        TreeSelection selection;
        std::deque<std::uint32_t> recent;
        PluginQuery query;
        float scroll{};
        float resultsWidth{};
        bool restoreScroll{};

        void TrackRecent(std::uint32_t id, std::size_t limit)
        {
            if (!id) return;
            std::erase(recent, id);
            recent.push_front(id);
            while (recent.size() > limit) recent.pop_back();
        }
        void ResetSession()
        {
            filterEditor = {};
            selection.Clear();
            recent.clear();
            diagnosticsPlugin.clear();
            query.Clear();
            spawnQuantity = 1;
            detailsFormID = 0;
            detailsPlugin.clear();
            admission = ActionAdmission::Accepted;
            contextQuantities.clear();
        }
    };
}
