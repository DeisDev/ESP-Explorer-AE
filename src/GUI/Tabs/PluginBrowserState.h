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
        std::array<char, 256> searchBuffer{};
        std::string diagnosticsPlugin;
        bool focusPending{};
        bool showUnknown{};
        bool globalSearch{};
        bool collapseDiagnostics{};
        int spawnQuantity{ 1 };
        ActionAdmission admission{ ActionAdmission::Accepted };
        std::unordered_map<std::uint32_t, int> contextQuantities;
        TreeSelection selection;
        std::deque<std::uint32_t> recent;
        PluginQuery query;

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
            admission = ActionAdmission::Accepted;
            contextQuantities.clear();
        }
    };
}
