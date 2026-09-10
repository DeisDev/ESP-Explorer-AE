#include "GUI/Tabs/ObjectBrowserTab.h"
#include "GUI/Widgets/BrowserWidgets.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void ObjectBrowserTab::Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests)
    {
        BrowserWidgets::DrawControls(state, view, requests, "ObjectBrowser", "Objects", "Object Search");
        struct Category { const char* id; const char* table; const char* section; const char* key; const char* fallback; std::vector<std::string> types; };
        static const Category categories[]{
            { "ObjectAll", "ObjectTableAll", "General", "sAll", "All", { "ACTI", "CONT", "STAT", "FURN" } },
            { "ACTI", "ObjectTableActivators", "Objects", "sActivators", "Activators", {} },
            { "CONT", "ObjectTableContainers", "Objects", "sContainers", "Containers", {} },
            { "STAT", "ObjectTableStatics", "Objects", "sStatics", "Statics", {} },
            { "FURN", "ObjectTableFurniture", "Objects", "sFurniture", "Furniture", {} }
        };
        if (!ImGui::BeginTabBar("ObjectCategories")) return;
        for (const auto& category : categories) {
            const auto label = std::string(view.localize(category.section, category.key, category.fallback)) + "###" + category.id;
            const bool open = ImGui::BeginTabItem(label.c_str());
            SharedUtils::DrawCurrentItemChrome(open, ImGui::IsItemHovered(), true, false);
            if (!open) continue;
            BrowserWidgets::ActivateCategory(state, view, category.id);
            const FormTableConfig config{
                .tableId = category.table, .primaryActionLabel = view.localize("Objects", "sPlace", "Place"),
                .allowFavorites = true, .gameplayActionsAllowed = view.gameplayReady
            };
            BrowserWidgets::DrawCategory(state, view, requests, category.id, config, ActionKind::Place, {}, category.types);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
