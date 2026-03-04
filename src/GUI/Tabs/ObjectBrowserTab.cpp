#include "GUI/Tabs/ObjectBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFilterCache.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        const std::vector<FormEntry>& GetFilteredEntries(
            RecordFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            const ObjectBrowserTab::FilterEntriesFn& filterEntries)
        {
            return RecordFilterCache::GetFiltered(cacheState, source, showPlayable, showNonPlayable, showNamed, showUnnamed, showDeleted, filterEntries);
        }
    }

    void ObjectBrowserTab::Draw(
        const FormCache& cache,
        char* searchBuffer,
        std::size_t searchBufferSize,
        std::string& searchText,
        std::string_view selectedPluginFilter,
        bool& showPlayableRecords,
        bool& showNonPlayableRecords,
        bool& showNamedRecords,
        bool& showUnnamedRecords,
        bool& showDeletedRecords,
        bool* searchFocusPending,
        std::unordered_set<std::uint32_t>& favoriteForms,
        const std::function<void()>& drawPluginFilterStatus,
        const std::function<void()>& persistListFilters,
        const std::function<void()>&,
        const FilterEntriesFn& filterEntries,
        const LocalizeFn& localize,
        const ContextMenuCallbacks* contextCallbacks)
    {
        if (RecordFiltersWidget::Draw(
                localize,
                "ObjectBrowser",
                RecordFilterState{
                    .showNonPlayable = showNonPlayableRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords })) {
            persistListFilters();
        }

        SearchBar::Draw(localize("Objects", "sSearch", "Object Search"), searchBuffer, searchBufferSize, searchText, searchFocusPending);
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ObjectCategories")) {
            static RecordFilterCache activatorFilterCache{};
            static RecordFilterCache containerFilterCache{};
            static RecordFilterCache staticFilterCache{};
            static RecordFilterCache furnitureFilterCache{};

            const auto& filteredActivators = GetFilteredEntries(
                activatorFilterCache,
                cache.activators,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                filterEntries);
            const auto& filteredContainers = GetFilteredEntries(
                containerFilterCache,
                cache.containers,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                filterEntries);
            const auto& filteredStatics = GetFilteredEntries(
                staticFilterCache,
                cache.statics,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                filterEntries);
            const auto& filteredFurniture = GetFilteredEntries(
                furnitureFilterCache,
                cache.furniture,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                filterEntries);

            const FormTableConfig activatorConfig{
                .tableId = "ObjectTableActivators",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = nullptr,
                .allowFavorites = true
            };
            const FormTableConfig containerConfig{
                .tableId = "ObjectTableContainers",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = nullptr,
                .allowFavorites = true
            };
            const FormTableConfig staticConfig{
                .tableId = "ObjectTableStatics",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = nullptr,
                .allowFavorites = true
            };
            const FormTableConfig furnitureConfig{
                .tableId = "ObjectTableFurniture",
                .primaryActionLabel = localize("Objects", "sPlace", "Place"),
                .quantityActionLabel = nullptr,
                .allowFavorites = true
            };

            auto placeAction = [](const FormEntry& entry) {
                FormActions::PlaceAtPlayer(entry.formID, 1);
            };

            if (ImGui::BeginTabItem(localize("Objects", "sActivators", "Activators"))) {
                FormTable::Draw(
                    filteredActivators,
                    searchText,
                    selectedPluginFilter,
                    activatorConfig,
                    placeAction,
                    {},
                    {},
                    &favoriteForms,
                    contextCallbacks);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Objects", "sContainers", "Containers"))) {
                FormTable::Draw(
                    filteredContainers,
                    searchText,
                    selectedPluginFilter,
                    containerConfig,
                    placeAction,
                    {},
                    {},
                    &favoriteForms,
                    contextCallbacks);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Objects", "sStatics", "Statics"))) {
                FormTable::Draw(
                    filteredStatics,
                    searchText,
                    selectedPluginFilter,
                    staticConfig,
                    placeAction,
                    {},
                    {},
                    &favoriteForms,
                    contextCallbacks);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Objects", "sFurniture", "Furniture"))) {
                FormTable::Draw(
                    filteredFurniture,
                    searchText,
                    selectedPluginFilter,
                    furnitureConfig,
                    placeAction,
                    {},
                    {},
                    &favoriteForms,
                    contextCallbacks);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
