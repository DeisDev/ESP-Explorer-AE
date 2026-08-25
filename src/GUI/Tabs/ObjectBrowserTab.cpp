#include "GUI/Tabs/ObjectBrowserTab.h"
#include "GUI/Widgets/BrowserWidgets.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void ObjectBrowserTab::Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests)
    {
        BrowserWidgets::DrawControls(state, view, requests, "ObjectBrowser", "Objects", "Object Search");
        struct Category { const char* type; const char* table; const char* key; const char* fallback; };
        static constexpr Category categories[]{
            { "ACTI", "ObjectTableActivators", "sActivators", "Activators" },
            { "CONT", "ObjectTableContainers", "sContainers", "Containers" },
            { "STAT", "ObjectTableStatics", "sStatics", "Statics" },
            { "FURN", "ObjectTableFurniture", "sFurniture", "Furniture" }
        };
        if (!ImGui::BeginTabBar("ObjectCategories")) return;
        for (const auto& category : categories) {
            const auto label = std::string(view.localize("Objects", category.key, category.fallback)) + "###" + category.type;
            const bool open = ImGui::BeginTabItem(label.c_str());
            SharedUtils::DrawCurrentItemChrome(open, ImGui::IsItemHovered(), true, false);
            if (!open) continue;
            BrowserWidgets::ActivateCategory(state, view, category.type);
            const FormTableConfig config{
                .tableId = category.table, .primaryActionLabel = view.localize("Objects", "sPlace", "Place"),
                .allowFavorites = true, .gameplayActionsAllowed = view.gameplayReady
            };
            BrowserWidgets::DrawCategory(state, view, requests, category.type, config, ActionKind::Place);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
