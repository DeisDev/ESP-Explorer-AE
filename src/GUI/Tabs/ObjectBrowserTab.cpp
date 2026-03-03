#include "GUI/Tabs/ObjectBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        struct LocalFilterCache
        {
            const std::vector<FormEntry>* source{ nullptr };
            std::size_t sourceSize{ 0 };
            bool showPlayable{ true };
            bool showNonPlayable{ true };
            bool showNamed{ true };
            bool showUnnamed{ true };
            bool showDeleted{ true };
            std::vector<FormEntry> filtered{};
        };

        const std::vector<FormEntry>& GetFilteredEntries(
            LocalFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            const ObjectBrowserTab::FilterEntriesFn& filterEntries)
        {
            const bool needsRebuild =
                cacheState.source != &source ||
                cacheState.sourceSize != source.size() ||
                cacheState.showPlayable != showPlayable ||
                cacheState.showNonPlayable != showNonPlayable ||
                cacheState.showNamed != showNamed ||
                cacheState.showUnnamed != showUnnamed ||
                cacheState.showDeleted != showDeleted;

            if (needsRebuild) {
                cacheState.filtered = filterEntries(source);
                cacheState.source = &source;
                cacheState.sourceSize = source.size();
                cacheState.showPlayable = showPlayable;
                cacheState.showNonPlayable = showNonPlayable;
                cacheState.showNamed = showNamed;
                cacheState.showUnnamed = showUnnamed;
                cacheState.showDeleted = showDeleted;
            }

            return cacheState.filtered;
        }
    }

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
        const std::function<void()>& persistFilterCheckboxes,
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
            if (ImGui::Checkbox(localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &searchCaseSensitive)) {
                persistFilterCheckboxes();
            }
            ImGui::EndTable();
        }
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ObjectCategories")) {
            static LocalFilterCache activatorFilterCache{};
            static LocalFilterCache containerFilterCache{};
            static LocalFilterCache staticFilterCache{};
            static LocalFilterCache furnitureFilterCache{};

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
                    filteredActivators,
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
                    filteredContainers,
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
                    filteredStatics,
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
                    filteredFurniture,
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
