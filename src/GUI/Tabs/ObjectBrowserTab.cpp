#include "GUI/Tabs/ObjectBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void ObjectBrowserTab::Draw(
        const FormCache& cache,
        char* searchBuffer,
        std::size_t searchBufferSize,
        std::string& searchText,
        bool& searchCaseSensitive,
        std::string_view selectedPluginFilter,
        bool& showPlayableRecords,
        bool& showNonPlayableRecords,
        bool& showNamedRecords,
        bool& showUnnamedRecords,
        bool& showDeletedRecords,
        std::unordered_set<std::uint32_t>& favoriteForms,
        const std::function<void()>& drawPluginFilterStatus,
        const std::function<void()>& persistListFilters,
        const FilterEntriesFn& filterEntries,
        const LocalizeFn& localize)
    {
        if (RecordFiltersWidget::Draw(
                localize,
                "ObjectBrowser",
                RecordFilterState{
                    .showPlayable = showPlayableRecords,
                    .showNonPlayable = showNonPlayableRecords,
                    .showNamed = showNamedRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords })) {
            persistListFilters();
        }

        if (ImGui::BeginTable("ObjectSearchRow", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 0.78f);
            ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 0.22f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            SearchBar::Draw(localize("Objects", "sSearch", "Object Search"), searchBuffer, searchBufferSize, searchText);
            ImGui::TableNextColumn();
            ImGui::Checkbox(localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &searchCaseSensitive);
            ImGui::EndTable();
        }
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ObjectCategories")) {
            const FormTableConfig activatorConfig{
                .tableId = "ObjectTableActivators",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = localize("Objects", "sPlaceAtPlayer", "Place At Player"),
                .allowFavorites = true
            };
            const FormTableConfig containerConfig{
                .tableId = "ObjectTableContainers",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = localize("Objects", "sPlaceAtPlayer", "Place At Player"),
                .allowFavorites = true
            };
            const FormTableConfig staticConfig{
                .tableId = "ObjectTableStatics",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = localize("Objects", "sPlaceAtPlayer", "Place At Player"),
                .allowFavorites = true
            };
            const FormTableConfig furnitureConfig{
                .tableId = "ObjectTableFurniture",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = localize("Objects", "sPlaceAtPlayer", "Place At Player"),
                .allowFavorites = true
            };

            if (ImGui::BeginTabItem(localize("Objects", "sActivators", "Activators"))) {
                FormTable::Draw(
                    filterEntries(cache.activators),
                    searchText,
                    selectedPluginFilter,
                    activatorConfig,
                    [](const FormEntry& entry) {
                        FormActions::PlaceAtPlayer(entry.formID, 1);
                    },
                    [](const FormEntry& entry, int quantity) {
                        FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                    },
                    &favoriteForms,
                    &searchCaseSensitive);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Objects", "sContainers", "Containers"))) {
                FormTable::Draw(
                    filterEntries(cache.containers),
                    searchText,
                    selectedPluginFilter,
                    containerConfig,
                    [](const FormEntry& entry) {
                        FormActions::PlaceAtPlayer(entry.formID, 1);
                    },
                    [](const FormEntry& entry, int quantity) {
                        FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                    },
                    &favoriteForms,
                    &searchCaseSensitive);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Objects", "sStatics", "Statics"))) {
                FormTable::Draw(
                    filterEntries(cache.statics),
                    searchText,
                    selectedPluginFilter,
                    staticConfig,
                    [](const FormEntry& entry) {
                        FormActions::PlaceAtPlayer(entry.formID, 1);
                    },
                    [](const FormEntry& entry, int quantity) {
                        FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                    },
                    &favoriteForms,
                    &searchCaseSensitive);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Objects", "sFurniture", "Furniture"))) {
                FormTable::Draw(
                    filterEntries(cache.furniture),
                    searchText,
                    selectedPluginFilter,
                    furnitureConfig,
                    [](const FormEntry& entry) {
                        FormActions::PlaceAtPlayer(entry.formID, 1);
                    },
                    [](const FormEntry& entry, int quantity) {
                        FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                    },
                    &favoriteForms,
                    &searchCaseSensitive);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
