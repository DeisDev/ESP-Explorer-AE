#include "Config/Config.h"
#include "GUI/Tabs/ObjectBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFilterCache.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"

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
            std::uint64_t advancedFilterRevision,
            const ObjectBrowserTab::FilterEntriesFn& filterEntries)
        {
            return RecordFilterCache::GetFiltered(cacheState, source, showPlayable, showNonPlayable, showNamed, showUnnamed, showDeleted, advancedFilterRevision, filterEntries);
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
        std::vector<AdvancedFilterRule>& advancedRules,
        std::unordered_set<std::string>& hiddenPlugins,
        std::uint64_t advancedFilterRevision,
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
                    .showDeleted = showDeletedRecords,
                    .advancedRules = advancedRules,
                    .hiddenPlugins = hiddenPlugins })) {
            persistListFilters();
        }

        SearchBar::Draw(localize("Objects", "sSearch", "Object Search"), searchBuffer, searchBufferSize, searchText, searchFocusPending);
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ObjectCategories")) {
            static int lastActiveCategory = -1;
            int activeCategory = -1;
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
                advancedFilterRevision,
                filterEntries);
            const auto& filteredContainers = GetFilteredEntries(
                containerFilterCache,
                cache.containers,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedFilterRevision,
                filterEntries);
            const auto& filteredStatics = GetFilteredEntries(
                staticFilterCache,
                cache.statics,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedFilterRevision,
                filterEntries);
            const auto& filteredFurniture = GetFilteredEntries(
                furnitureFilterCache,
                cache.furniture,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedFilterRevision,
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

            const bool activatorsTabOpen = ImGui::BeginTabItem(localize("Objects", "sActivators", "Activators"));
            SharedUtils::DrawCurrentItemChrome(activatorsTabOpen, ImGui::IsItemHovered(), true, false);
            if (activatorsTabOpen) {
                activeCategory = 0;
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

            const bool containersTabOpen = ImGui::BeginTabItem(localize("Objects", "sContainers", "Containers"));
            SharedUtils::DrawCurrentItemChrome(containersTabOpen, ImGui::IsItemHovered(), true, false);
            if (containersTabOpen) {
                activeCategory = 1;
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

            const bool staticsTabOpen = ImGui::BeginTabItem(localize("Objects", "sStatics", "Statics"));
            SharedUtils::DrawCurrentItemChrome(staticsTabOpen, ImGui::IsItemHovered(), true, false);
            if (staticsTabOpen) {
                activeCategory = 2;
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

            const bool furnitureTabOpen = ImGui::BeginTabItem(localize("Objects", "sFurniture", "Furniture"));
            SharedUtils::DrawCurrentItemChrome(furnitureTabOpen, ImGui::IsItemHovered(), true, false);
            if (furnitureTabOpen) {
                activeCategory = 3;
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

            if (activeCategory != -1) {
                if (searchFocusPending && lastActiveCategory != -1 && activeCategory != lastActiveCategory && Config::Get().autoFocusSearchBars) {
                    *searchFocusPending = true;
                }
                lastActiveCategory = activeCategory;
            }
        }
    }
}
