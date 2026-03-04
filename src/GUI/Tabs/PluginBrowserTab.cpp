#include "GUI/Tabs/PluginBrowserTab.h"

#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/RecordFiltersWidget.h"

#include <imgui.h>

#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESSound.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWeightForm.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        bool PassesLocalRecordFilters(const FormEntry& entry, const PluginBrowserTabContext& context)
        {
            if (!context.showPlayableRecords && entry.isPlayable) {
                return false;
            }
            if (!context.showNonPlayableRecords && !entry.isPlayable) {
                return false;
            }

            const bool hasName = !entry.name.empty();
            if (!context.showNamedRecords && hasName) {
                return false;
            }
            if (!context.showUnnamedRecords && !hasName) {
                return false;
            }

            if (!context.showDeletedRecords && entry.isDeleted) {
                return false;
            }

            return true;
        }

        bool ContainsCaseInsensitive(std::string_view text, std::string_view query)
        {
            if (query.empty()) {
                return true;
            }

            std::string textLower(text.begin(), text.end());
            std::string queryLower(query.begin(), query.end());

            std::ranges::transform(textLower, textLower.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            std::ranges::transform(queryLower, queryLower.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });

            return textLower.find(queryLower) != std::string::npos;
        }

        bool ContainsByMode(std::string_view text, std::string_view query, bool caseSensitive)
        {
            if (query.empty()) {
                return true;
            }

            if (caseSensitive) {
                return text.find(query) != std::string::npos;
            }

            return ContainsCaseInsensitive(text, query);
        }

        const char* TryGetEditorID(std::uint32_t formID)
        {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (!form) {
                return nullptr;
            }

            const auto* editorID = form->GetFormEditorID();
            if (!editorID || editorID[0] == '\0') {
                return nullptr;
            }

            return editorID;
        }

        bool MatchesPluginSearch(const FormEntry& entry, std::string_view query, bool caseSensitive)
        {
            if (query.empty()) {
                return true;
            }

            char formIDBuffer[16]{};
            std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);

            if (ContainsByMode(entry.name, query, caseSensitive) ||
                ContainsByMode(entry.category, query, caseSensitive) ||
                ContainsByMode(entry.sourcePlugin, query, caseSensitive) ||
                ContainsByMode(formIDBuffer, query, caseSensitive)) {
                return true;
            }

            const char* editorID = TryGetEditorID(entry.formID);
            return editorID && ContainsByMode(editorID, query, caseSensitive);
        }

        bool CanGiveFromTreeCategory(std::string_view category)
        {
            return category == "Weapon" || category == "Armor" || category == "Ammo" || category == "Misc" ||
                   category == "WEAP" || category == "ARMO" || category == "AMMO" || category == "MISC" ||
                   category == "ALCH" || category == "BOOK" || category == "KEYM" || category == "NOTE" ||
                   category == "INGR" || category == "CMPO" || category == "OMOD";
        }

        bool CanSpawnFromTreeCategory(std::string_view category)
        {
            return category == "NPC" || category == "NPC_" || category == "LVLN" ||
                   category == "Activator" || category == "Container" || category == "Static" || category == "Furniture" ||
                   category == "ACTI" || category == "CONT" || category == "STAT" || category == "FURN" ||
                   category == "LIGH" || category == "FLOR" || category == "TREE";
        }

        bool CanTeleportFromTreeCategory(std::string_view category)
        {
            return category == "CELL" || category == "WRLD" || category == "LCTN" || category == "REGN";
        }

        bool IsQuestCategory(std::string_view category)
        {
            return category == "QUST" || category == "Quest";
        }

        bool IsPerkCategory(std::string_view category)
        {
            return category == "PERK" || category == "Perk";
        }

        bool IsDataCategory(std::string_view category)
        {
            return category == "KYWD" || category == "FLST" || category == "GLOB" || category == "COBJ";
        }

        bool IsUnknownCategory(std::string_view category)
        {
            if (category.empty()) {
                return true;
            }

            std::string lowered(category.begin(), category.end());
            std::ranges::transform(lowered, lowered.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });

            return lowered == "unknown" || lowered == "<unknown>";
        }

        bool IsSpellLikeCategory(std::string_view category)
        {
            return category == "SPEL" || category == "Spell" || category == "MGEF" || category == "Effect";
        }

        bool IsWeatherCategory(std::string_view category)
        {
            return category == "WTHR" || category == "Weather";
        }

        bool IsSoundCategory(std::string_view category)
        {
            return category == "SOUN" || category == "SNDR";
        }

        bool IsGlobalCategory(std::string_view category)
        {
            return category == "GLOB";
        }

        bool IsOutfitCategory(std::string_view category)
        {
            return category == "OTFT";
        }

        bool IsConstructibleCategory(std::string_view category)
        {
            return category == "COBJ";
        }

        std::string CategoryDisplayName(std::string_view category, const PluginBrowserTabContext& context)
        {
            if (category == "WEAP" || category == "Weapon") {
                return context.localize("Items", "sWeapons", "Weapons");
            }
            if (category == "ARMO" || category == "Armor") {
                return context.localize("Items", "sArmor", "Armor");
            }
            if (category == "AMMO" || category == "Ammo") {
                return context.localize("General", "sAmmunition", "Ammunition");
            }
            if (category == "ALCH") {
                return context.localize("General", "sAidChems", "Aid/Chems");
            }
            if (category == "BOOK") {
                return context.localize("General", "sBooks", "Books");
            }
            if (category == "MISC" || category == "Misc") {
                return context.localize("General", "sMiscellaneous", "Miscellaneous");
            }
            if (category == "KEYM") {
                return context.localize("General", "sKeys", "Keys");
            }
            if (category == "NOTE") {
                return context.localize("General", "sHolotapesNotes", "Holotapes/Notes");
            }
            if (category == "NPC" || category == "NPC_") {
                return context.localize("NPCs", "sTabName", "NPCs");
            }
            if (category == "LVLN") {
                return context.localize("General", "sLeveledNPCs", "Leveled NPCs");
            }
            if (category == "ACTI" || category == "Activator") {
                return context.localize("Objects", "sActivators", "Activators");
            }
            if (category == "CONT" || category == "Container") {
                return context.localize("Objects", "sContainers", "Containers");
            }
            if (category == "STAT" || category == "Static") {
                return context.localize("General", "sStaticObjects", "Static Objects");
            }
            if (category == "FURN" || category == "Furniture") {
                return context.localize("Objects", "sFurniture", "Furniture");
            }
            if (category == "SPEL" || category == "Spell") {
                return context.localize("Spells", "sSpells", "Spells");
            }
            if (category == "PERK" || category == "Perk") {
                return context.localize("Spells", "sPerks", "Perks");
            }
            if (category == "SNDR") {
                return context.localize("General", "sSoundDescriptors", "Sound Descriptors");
            }
            if (category == "SOUN") {
                return context.localize("General", "sSounds", "Sounds");
            }
            return std::string(category);
        }

        ImVec4 CategoryColor(std::string_view category)
        {
            if (CanGiveFromTreeCategory(category)) {
                return ImVec4(0.40f, 0.80f, 0.40f, 1.00f);
            }
            if (CanSpawnFromTreeCategory(category)) {
                return ImVec4(0.82f, 0.62f, 0.38f, 1.00f);
            }
            if (category == "CELL" || category == "WRLD" || category == "LCTN" || category == "REGN") {
                return ImVec4(0.42f, 0.62f, 0.88f, 1.00f);
            }
            if (category == "WTHR") {
                return ImVec4(0.62f, 0.80f, 0.92f, 1.00f);
            }
            if (IsDataCategory(category)) {
                return ImVec4(0.80f, 0.72f, 0.52f, 1.00f);
            }
            return ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
        }

        const FormEntry* FindRecordByFormID(const FormCache& cache, std::uint32_t formID, std::uint64_t dataVersion)
        {
            static std::uint64_t indexedVersion{ 0 };
            static std::unordered_map<std::uint32_t, const FormEntry*> formIDIndex{};

            if (indexedVersion != dataVersion) {
                formIDIndex.clear();
                formIDIndex.reserve(cache.allRecords.size());
                for (const auto& entry : cache.allRecords) {
                    formIDIndex[entry.formID] = &entry;
                }

                indexedVersion = dataVersion;
            }

            const auto it = formIDIndex.find(formID);
            return it != formIDIndex.end() ? it->second : nullptr;
        }

        void TrackRecentRecord(std::uint32_t formID, PluginBrowserTabContext& context)
        {
            if (formID == 0) {
                return;
            }

            context.recentPluginRecordFormIDs.erase(
                std::remove(context.recentPluginRecordFormIDs.begin(), context.recentPluginRecordFormIDs.end(), formID),
                context.recentPluginRecordFormIDs.end());
            context.recentPluginRecordFormIDs.push_front(formID);

            constexpr std::size_t kMaxRecentRecords = 50;
            while (context.recentPluginRecordFormIDs.size() > kMaxRecentRecords) {
                context.recentPluginRecordFormIDs.pop_back();
            }
        }

        void EnsurePrimarySelectionValid(PluginBrowserTabContext& context)
        {
            if (context.selectedPluginTreeRecordFormID != 0 && context.selectedPluginTreeRecordFormIDs.contains(context.selectedPluginTreeRecordFormID)) {
                return;
            }

            if (context.selectedPluginTreeRecordFormIDs.empty()) {
                context.selectedPluginTreeRecordFormID = 0;
                return;
            }

            context.selectedPluginTreeRecordFormID = *context.selectedPluginTreeRecordFormIDs.begin();
        }

        std::vector<FormEntry> CollectSelectedGiveableEntries(const FormCache& cache, std::uint64_t dataVersion, const PluginBrowserTabContext& context)
        {
            std::vector<FormEntry> selectedEntries{};
            selectedEntries.reserve(context.selectedPluginTreeRecordFormIDs.size());

            for (const auto selectedFormID : context.selectedPluginTreeRecordFormIDs) {
                const auto* selectedEntry = FindRecordByFormID(cache, selectedFormID, dataVersion);
                if (!selectedEntry || !CanGiveFromTreeCategory(selectedEntry->category)) {
                    continue;
                }

                selectedEntries.push_back(*selectedEntry);
            }

            return selectedEntries;
        }

        void EquipRecordWithConfiguredAmmo(const FormEntry& record, int ammoCount)
        {
            char command[64]{};
            std::snprintf(command, sizeof(command), "player.equipitem %08X", record.formID);
            FormActions::ExecuteConsoleCommand(command);

            const bool isWeapon = record.category == "WEAP" || record.category == "Weapon";
            if (!isWeapon || ammoCount <= 0) {
                return;
            }

            const auto ammoFormID = FormActions::GetWeaponAmmoFormID(record.formID);
            if (ammoFormID != 0) {
                FormActions::GiveToPlayer(ammoFormID, static_cast<std::uint32_t>(ammoCount));
            }
        }

        std::unordered_map<std::uint32_t, int> pluginSpawnQuantities{};

        ContextMenuCallbacks BuildContextCallbacks(PluginBrowserTabContext& context)
        {
            ContextMenuCallbacks callbacks{};
            callbacks.localize = context.localize;
            callbacks.openItemGrantPopup = context.openItemGrantPopup;
            callbacks.openGlobalValuePopup = context.openGlobalValuePopup;
            callbacks.requestActionConfirmation = context.requestActionConfirmation;
            callbacks.favorites = &context.favoriteForms;
            callbacks.equipWeaponAmmoCount = context.equipWeaponAmmoCount;
            return callbacks;
        }

        void DrawRecordContextMenu(const FormEntry& record, bool isSelected, PluginBrowserTabContext& context)
        {
            if (ImGui::MenuItem(isSelected ? context.localize("General", "sDeselect", "Deselect") : context.localize("General", "sSelect", "Select"))) {
                if (isSelected) {
                    context.selectedPluginTreeRecordFormIDs.erase(record.formID);
                } else {
                    context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                    context.selectedPluginTreeRecordFormID = record.formID;
                }
                EnsurePrimarySelectionValid(context);
            }

            ImGui::Separator();

            auto callbacks = BuildContextCallbacks(context);
            ContextMenu::Draw(record, callbacks);
        }
    }

    void PluginBrowserTab::Draw(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
        bool listFilterSettingsChanged = false;

        const float clearBtnWidth = ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float searchFieldWidth = ImGui::GetContentRegionAvail().x * 0.55f - clearBtnWidth - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(searchFieldWidth);
        if (context.searchFocusPending && *context.searchFocusPending && !ImGui::IsAnyItemActive() && !ImGui::GetIO().NavActive) {
            ImGui::SetKeyboardFocusHere();
            *context.searchFocusPending = false;
        }
        if (ImGui::InputTextWithHint("##PluginSearchInput", context.localize("PluginBrowser", "sSearch", "Plugin Search"), context.pluginSearchBuffer, context.pluginSearchBufferSize)) {
            context.pluginSearch = context.pluginSearchBuffer;
        }
        ImGui::SameLine();
        if (ImGui::Button("X##PluginSearchClear")) {
            context.pluginSearchBuffer[0] = '\0';
            context.pluginSearch.clear();
        }
        ImGui::SameLine();

        if (ImGui::Button(context.localize("PluginBrowser", "sClearFilter", "Clear Plugin Filter"))) {
            context.selectedPluginFilter.clear();
        }

        if (ImGui::Checkbox(context.localize("PluginBrowser", "sGlobalSearch", "Global Search"), &context.pluginGlobalSearchMode)) {
            context.persistFilterCheckboxes();
        }
        {
            auto wrappedSameLine = [](const char* label) {
                float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().ItemSpacing.x;
                if (ImGui::GetContentRegionAvail().x >= w) {
                    ImGui::SameLine();
                }
            };
            const char* unknownLabel = context.localize("PluginBrowser", "sShowUnknownCategories", "Show Unknown Categories");
            wrappedSameLine(unknownLabel);
            if (ImGui::Checkbox(unknownLabel, &context.showUnknownCategories)) {
                listFilterSettingsChanged = true;
                context.persistFilterCheckboxes();
            }
        }

        if (!context.selectedPluginFilter.empty()) {
            ImGui::Text("%s: %s", context.localize("PluginBrowser", "sActiveFilter", "Active"), context.selectedPluginFilter.c_str());
        }

        if (RecordFiltersWidget::Draw(
                context.localize,
                "PluginBrowser",
                RecordFilterState{
                    .showNonPlayable = context.showNonPlayableRecords,
                    .showUnnamed = context.showUnnamedRecords,
                    .showDeleted = context.showDeletedRecords })) {
            listFilterSettingsChanged = true;
        }

        if (listFilterSettingsChanged) {
            context.persistListFilters();
        }

        const bool needsCacheRebuild =
            context.pluginBrowserCacheVersion != dataVersion ||
            context.pluginBrowserCacheSearch != context.pluginSearch ||
            context.pluginBrowserCacheSelectedPlugin != context.selectedPluginFilter ||
            context.pluginBrowserCacheShowPlayable != context.showPlayableRecords ||
            context.pluginBrowserCacheShowNonPlayable != context.showNonPlayableRecords ||
            context.pluginBrowserCacheShowNamed != context.showNamedRecords ||
            context.pluginBrowserCacheShowUnnamed != context.showUnnamedRecords ||
            context.pluginBrowserCacheShowDeleted != context.showDeletedRecords ||
            context.pluginBrowserCacheShowUnknown != context.showUnknownCategories ||
            context.pluginBrowserCacheGlobalSearchMode != context.pluginGlobalSearchMode;

        if (needsCacheRebuild) {
            context.pluginBrowserGroupedRecordsCache.clear();
            context.pluginBrowserGroupedRecordsCache.reserve(plugins.size());
            context.pluginBrowserGlobalSearchResultsCache.clear();
            context.pluginBrowserGlobalSearchResultsCache.reserve(cache.allRecords.size());

            for (const auto& entry : cache.allRecords) {
                if (!PassesLocalRecordFilters(entry, context)) {
                    continue;
                }

                if (!context.showUnknownCategories && entry.sourcePlugin.empty()) {
                    continue;
                }

                if (!context.showUnknownCategories && IsUnknownCategory(entry.category)) {
                    continue;
                }

                const std::string pluginName = entry.sourcePlugin.empty() ? std::string(context.localize("General", "sUnknown", "<Unknown>")) : entry.sourcePlugin;

                const bool searchMatches = MatchesPluginSearch(entry, context.pluginSearch, false);

                if (context.pluginGlobalSearchMode && !context.pluginSearch.empty() && searchMatches) {
                    context.pluginBrowserGlobalSearchResultsCache.push_back(&entry);
                }

                if (!context.pluginGlobalSearchMode && !context.selectedPluginFilter.empty() && pluginName != context.selectedPluginFilter) {
                    continue;
                }

                if (!context.pluginSearch.empty() && !searchMatches) {
                    continue;
                }

                context.pluginBrowserGroupedRecordsCache[pluginName][entry.category].push_back(&entry);
            }

            context.pluginBrowserOrderedPluginsCache.clear();
            context.pluginBrowserOrderedPluginsCache.reserve(context.pluginBrowserGroupedRecordsCache.size());
            std::unordered_set<std::string> seenPlugins{};
            seenPlugins.reserve(context.pluginBrowserGroupedRecordsCache.size());

            for (const auto& plugin : plugins) {
                if (context.pluginBrowserGroupedRecordsCache.contains(plugin.filename)) {
                    context.pluginBrowserOrderedPluginsCache.push_back(plugin.filename);
                    seenPlugins.insert(plugin.filename);
                }
            }

            for (const auto& [pluginName, _] : context.pluginBrowserGroupedRecordsCache) {
                if (!seenPlugins.contains(pluginName)) {
                    context.pluginBrowserOrderedPluginsCache.push_back(pluginName);
                }
            }

            context.pluginBrowserCacheVersion = dataVersion;
            context.pluginBrowserCacheSearch = context.pluginSearch;
            context.pluginBrowserCacheSelectedPlugin = context.selectedPluginFilter;
            context.pluginBrowserCacheShowPlayable = context.showPlayableRecords;
            context.pluginBrowserCacheShowNonPlayable = context.showNonPlayableRecords;
            context.pluginBrowserCacheShowNamed = context.showNamedRecords;
            context.pluginBrowserCacheShowUnnamed = context.showUnnamedRecords;
            context.pluginBrowserCacheShowDeleted = context.showDeletedRecords;
            context.pluginBrowserCacheShowUnknown = context.showUnknownCategories;
            context.pluginBrowserCacheGlobalSearchMode = context.pluginGlobalSearchMode;
        }

        const float leftWidth = ImGui::GetContentRegionAvail().x * 0.58f;

        if (ImGui::BeginChild("PluginTreeLeft", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders)) {
            auto drawRecordSelectable = [&](const FormEntry& record, const char* idPrefix) {
                const auto* displayName = record.name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : record.name.c_str();
                char recordLabel[512]{};
                std::snprintf(recordLabel, sizeof(recordLabel), "%s [%08X]##%s%08X", displayName, record.formID, idPrefix, record.formID);
                const bool isSelected = context.selectedPluginTreeRecordFormIDs.contains(record.formID);
                if (ImGui::Selectable(recordLabel, isSelected)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (isSelected) {
                            context.selectedPluginTreeRecordFormIDs.erase(record.formID);
                        } else {
                            context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                            context.selectedPluginTreeRecordFormID = record.formID;
                            TrackRecentRecord(record.formID, context);
                        }
                        EnsurePrimarySelectionValid(context);
                    } else {
                        context.selectedPluginTreeRecordFormIDs.clear();
                        context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                        context.selectedPluginTreeRecordFormID = record.formID;
                        TrackRecentRecord(record.formID, context);
                    }
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (!isSelected) {
                        context.selectedPluginTreeRecordFormIDs.clear();
                        context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                        context.selectedPluginTreeRecordFormID = record.formID;
                        TrackRecentRecord(record.formID, context);
                    }

                    DrawRecordContextMenu(record, true, context);

                    ImGui::EndPopup();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && CanGiveFromTreeCategory(record.category)) {
                    context.openItemGrantPopup(record);
                }
            };

            const std::string globalResultsHeader = std::string(context.localize("PluginBrowser", "sGlobalSearchResults", "Global Search Results")) + "##PluginGlobalSearchResults";
            if (context.pluginGlobalSearchMode && !context.pluginSearch.empty() && ImGui::TreeNodeEx(globalResultsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(context.pluginBrowserGlobalSearchResultsCache.size()));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                        const auto* record = context.pluginBrowserGlobalSearchResultsCache[static_cast<std::size_t>(row)];
                        if (!record) {
                            continue;
                        }
                        drawRecordSelectable(*record, "GlobalResult");
                    }
                }
                ImGui::TreePop();
            }

            const std::string favoritesHeader = std::string(context.localize("General", "sFavorites", "Favorites")) + "##PluginFavorites";
            if (ImGui::TreeNodeEx(favoritesHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                for (const auto favoriteFormID : context.favoriteForms) {
                    const auto* entry = FindRecordByFormID(cache, favoriteFormID, dataVersion);
                    if (!entry) {
                        continue;
                    }
                    if (!PassesLocalRecordFilters(*entry, context)) {
                        continue;
                    }
                    if (!context.showUnknownCategories && (entry->sourcePlugin.empty() || IsUnknownCategory(entry->category))) {
                        continue;
                    }
                    if (!context.pluginSearch.empty() && !MatchesPluginSearch(*entry, context.pluginSearch, false)) {
                        continue;
                    }

                    drawRecordSelectable(*entry, "FavoriteRecord");
                }
                ImGui::TreePop();
            }

            const std::string recentRecordsHeader = std::string(context.localize("PluginBrowser", "sRecentRecords", "Recent Records")) + "##PluginRecentRecords";
            if (ImGui::TreeNodeEx(recentRecordsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                for (const auto recentFormID : context.recentPluginRecordFormIDs) {
                    const auto* recentEntry = FindRecordByFormID(cache, recentFormID, dataVersion);
                    if (!recentEntry) {
                        continue;
                    }
                    if (!PassesLocalRecordFilters(*recentEntry, context)) {
                        continue;
                    }
                    if (!context.showUnknownCategories && (recentEntry->sourcePlugin.empty() || IsUnknownCategory(recentEntry->category))) {
                        continue;
                    }
                    if (!context.pluginSearch.empty() && !MatchesPluginSearch(*recentEntry, context.pluginSearch, false)) {
                        continue;
                    }

                    drawRecordSelectable(*recentEntry, "RecentRecord");
                }
                ImGui::TreePop();
            }

            ImGui::Separator();

            for (const auto& pluginName : context.pluginBrowserOrderedPluginsCache) {
                auto pluginIt = context.pluginBrowserGroupedRecordsCache.find(pluginName);
                if (pluginIt == context.pluginBrowserGroupedRecordsCache.end()) {
                    continue;
                }

                std::size_t totalRecords = 0;
                for (const auto& [_, list] : pluginIt->second) {
                    totalRecords += list.size();
                }

                const std::string pluginLabel = pluginName + " (" + std::to_string(totalRecords) + ")";
                if (ImGui::TreeNode(pluginLabel.c_str())) {
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                        context.selectedPluginFilter = pluginName;
                    }

                    std::vector<std::string> categories;
                    categories.reserve(pluginIt->second.size());
                    for (const auto& [category, _] : pluginIt->second) {
                        categories.push_back(category);
                    }
                    std::ranges::sort(categories);

                    for (const auto& category : categories) {
                        auto categoryIt = pluginIt->second.find(category);
                        if (categoryIt == pluginIt->second.end()) {
                            continue;
                        }

                        const auto displayCategory = CategoryDisplayName(category, context);
                        std::string categoryLabel{};
                        if (displayCategory == category) {
                            categoryLabel = std::string(displayCategory) + " (" + std::to_string(categoryIt->second.size()) + ")";
                        } else {
                            categoryLabel = std::string(displayCategory) + " [" + category + "] (" + std::to_string(categoryIt->second.size()) + ")";
                        }
                        ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor(category));
                        if (ImGui::TreeNode(categoryLabel.c_str())) {
                            ImGuiListClipper clipper;
                            clipper.Begin(static_cast<int>(categoryIt->second.size()));
                            while (clipper.Step()) {
                                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                                    const auto* record = categoryIt->second[static_cast<std::size_t>(row)];
                                    if (!record) {
                                        continue;
                                    }
                                    drawRecordSelectable(*record, "TreeRecord");
                                }
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if (ImGui::BeginChild("PluginTreeDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            EnsurePrimarySelectionValid(context);
            const FormEntry* selectedRecord = context.selectedPluginTreeRecordFormID != 0 ? FindRecordByFormID(cache, context.selectedPluginTreeRecordFormID, dataVersion) : nullptr;
            const auto selectedGiveableEntries = CollectSelectedGiveableEntries(cache, dataVersion, context);
            const bool hasMultipleSelection = context.selectedPluginTreeRecordFormIDs.size() > 1;

            if (!selectedRecord) {
                ImGui::TextUnformatted(context.localize("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
            } else {
                int detailCopyPopupCounter = 0;
                auto drawCopyPopup = [&](std::string_view value) {
                    const std::string popupID = "DetailCopyPopup##" + std::to_string(detailCopyPopupCounter++);
                    if (ImGui::BeginPopupContextItem(popupID.c_str())) {
                        if (ImGui::MenuItem(context.localize("General", "sCopy", "Copy"))) {
                            ImGui::SetClipboardText(std::string(value).c_str());
                        }
                        ImGui::EndPopup();
                    }
                };

                const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 5.0f;
                if (ImGui::BeginChild("PluginDetailsInfo", ImVec2(0.0f, -footerHeight), false)) {

                ImGui::TextUnformatted(selectedRecord->name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedRecord->name.c_str());
                drawCopyPopup(selectedRecord->name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedRecord->name);
                ImGui::SameLine();
                ImGui::TextDisabled("%08X", selectedRecord->formID);
                char selectedFormIDBuffer[16]{};
                std::snprintf(selectedFormIDBuffer, sizeof(selectedFormIDBuffer), "%08X", selectedRecord->formID);
                drawCopyPopup(selectedFormIDBuffer);
                ImGui::Separator();

                ImGui::Text("%s: %s", context.localize("General", "sPlugin", "Plugin"), selectedRecord->sourcePlugin.c_str());
                drawCopyPopup(selectedRecord->sourcePlugin);
                const auto displayCategory = CategoryDisplayName(selectedRecord->category, context);
                if (displayCategory == selectedRecord->category) {
                    ImGui::Text("%s: %s", context.localize("General", "sCategory", "Category"), selectedRecord->category.c_str());
                    drawCopyPopup(selectedRecord->category);
                } else {
                    ImGui::Text("%s: %s [%s]", context.localize("General", "sCategory", "Category"), displayCategory.data(), selectedRecord->category.c_str());
                    drawCopyPopup(std::string(displayCategory) + " [" + selectedRecord->category + "]");
                }
                ImGui::Text("%s: %s", context.localize("General", "sPlayable", "Playable"), selectedRecord->isPlayable ? context.localize("General", "sYes", "Yes") : context.localize("General", "sNo", "No"));
                drawCopyPopup(selectedRecord->isPlayable ? context.localize("General", "sYes", "Yes") : context.localize("General", "sNo", "No"));
                ImGui::Text("%s: %s", context.localize("General", "sDeleted", "Deleted"), selectedRecord->isDeleted ? context.localize("General", "sYes", "Yes") : context.localize("General", "sNo", "No"));
                drawCopyPopup(selectedRecord->isDeleted ? context.localize("General", "sYes", "Yes") : context.localize("General", "sNo", "No"));

                auto* form = RE::TESForm::GetFormByID(selectedRecord->formID);
                if (form) {
                    const auto* editorID = form->GetFormEditorID();
                    ImGui::Text("%s: %s", context.localize("General", "sEditorID", "EditorID"), (editorID && editorID[0] != '\0') ? editorID : context.localize("General", "sNone", "None"));
                    drawCopyPopup((editorID && editorID[0] != '\0') ? std::string(editorID) : context.localize("General", "sNone", "None"));
                    ImGui::Text("%s: %s", context.localize("General", "sType", "Type"), form->GetFormTypeString());
                    drawCopyPopup(form->GetFormTypeString());
                    ImGui::Text("%s: %u", context.localize("General", "sReferenceCount", "Reference Count"), DataManager::GetPlacedReferenceCount(selectedRecord->formID));
                    {
                        char referenceCountBuffer[32]{};
                        std::snprintf(referenceCountBuffer, sizeof(referenceCountBuffer), "%u", DataManager::GetPlacedReferenceCount(selectedRecord->formID));
                        drawCopyPopup(referenceCountBuffer);
                    }

                    if (const auto* valueForm = form->As<RE::TESValueForm>()) {
                        ImGui::Text("%s: %d", context.localize("General", "sValue", "Value"), valueForm->GetFormValue());
                        char valueBuffer[32]{};
                        std::snprintf(valueBuffer, sizeof(valueBuffer), "%d", valueForm->GetFormValue());
                        drawCopyPopup(valueBuffer);
                    }

                    if (const auto* weightForm = form->As<RE::TESWeightForm>()) {
                        ImGui::Text("%s: %.2f", context.localize("General", "sWeight", "Weight"), weightForm->GetFormWeight());
                        char weightBuffer[32]{};
                        std::snprintf(weightBuffer, sizeof(weightBuffer), "%.2f", weightForm->GetFormWeight());
                        drawCopyPopup(weightBuffer);
                    }

                    if (const auto* weaponForm = form->As<RE::TESObjectWEAP>()) {
                        ImGui::Text("%s: %u", context.localize("General", "sDamage", "Damage"), weaponForm->weaponData.attackDamage);
                        char damageBuffer[32]{};
                        std::snprintf(damageBuffer, sizeof(damageBuffer), "%u", weaponForm->weaponData.attackDamage);
                        drawCopyPopup(damageBuffer);
                        if (weaponForm->weaponData.attackSeconds > 0.0f) {
                            ImGui::Text("%s: %.2f", context.localize("General", "sFireRate", "Fire Rate"), 1.0f / weaponForm->weaponData.attackSeconds);
                            char fireRateBuffer[32]{};
                            std::snprintf(fireRateBuffer, sizeof(fireRateBuffer), "%.2f", 1.0f / weaponForm->weaponData.attackSeconds);
                            drawCopyPopup(fireRateBuffer);
                        }
                        if (weaponForm->weaponData.ammo) {
                            const auto ammoFormID = weaponForm->weaponData.ammo->GetFormID();
                            auto* ammoForm = RE::TESForm::GetFormByID(ammoFormID);
                            std::string ammoName{};
                            if (ammoForm) {
                                const auto maybeName = RE::TESFullName::GetFullName(*ammoForm);
                                if (!maybeName.empty()) {
                                    ammoName = std::string(maybeName);
                                }
                            }
                            if (!ammoName.empty()) {
                                ImGui::Text("%s: %s", context.localize("Items", "sAmmo", "Ammo"), ammoName.c_str());
                                drawCopyPopup(ammoName);
                            } else {
                                ImGui::Text("%s: %08X", context.localize("Items", "sAmmo", "Ammo"), ammoFormID);
                                char ammoBuffer[16]{};
                                std::snprintf(ammoBuffer, sizeof(ammoBuffer), "%08X", ammoFormID);
                                drawCopyPopup(ammoBuffer);
                            }
                        }
                    }

                    if (const auto* armorForm = form->As<RE::TESObjectARMO>()) {
                        ImGui::Text("%s: %u", context.localize("General", "sArmorRating", "Armor Rating"), armorForm->armorData.rating);
                        char armorBuffer[32]{};
                        std::snprintf(armorBuffer, sizeof(armorBuffer), "%u", armorForm->armorData.rating);
                        drawCopyPopup(armorBuffer);
                    }

                    if (const auto* npcForm = form->As<RE::TESNPC>()) {
                        ImGui::Text("%s: %d", context.localize("General", "sLevel", "Level"), npcForm->GetLevel());
                        char levelBuffer[32]{};
                        std::snprintf(levelBuffer, sizeof(levelBuffer), "%d", npcForm->GetLevel());
                        drawCopyPopup(levelBuffer);
                        if (const auto* raceForm = npcForm->GetFormRace()) {
                            const auto raceName = RE::TESFullName::GetFullName(*raceForm);
                            if (!raceName.empty()) {
                                const std::string raceNameString{ raceName };
                                ImGui::Text("%s: %s", context.localize("General", "sRace", "Race"), raceNameString.c_str());
                                drawCopyPopup(raceNameString);
                            }
                        }
                    }

                    if (const auto* soundForm = form->As<RE::TESSound>()) {
                        if (soundForm->descriptor) {
                            const auto* descriptorEditorID = soundForm->descriptor->GetFormEditorID();
                            if (descriptorEditorID && descriptorEditorID[0] != '\0') {
                                ImGui::Text("%s: %s", context.localize("General", "sDescriptor", "Descriptor"), descriptorEditorID);
                                drawCopyPopup(descriptorEditorID);
                            } else {
                                ImGui::Text("%s: %08X", context.localize("General", "sDescriptor", "Descriptor"), soundForm->descriptor->GetFormID());
                                char descriptorBuffer[16]{};
                                std::snprintf(descriptorBuffer, sizeof(descriptorBuffer), "%08X", soundForm->descriptor->GetFormID());
                                drawCopyPopup(descriptorBuffer);
                            }
                        } else {
                            ImGui::TextDisabled("%s: %s", context.localize("General", "sDescriptor", "Descriptor"), context.localize("General", "sNone", "None"));
                            drawCopyPopup(context.localize("General", "sNone", "None"));
                        }
                    }

                    if (const auto keywordForm = form->As<RE::BGSKeywordForm>()) {
                        ImGui::Separator();
                        ImGui::TextUnformatted(context.localize("General", "sKeywords", "Keywords"));
                        int displayed = 0;
                        keywordForm->ForEachKeyword([&displayed, &context](RE::BGSKeyword* keyword) {
                            if (!keyword || displayed >= 40) {
                                return displayed >= 40 ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
                            }
                            const auto text = keyword->formEditorID.c_str();
                            const auto keywordLabel = (text && text[0] != '\0') ? text : context.localize("General", "sUnnamedKeyword", "<UnnamedKeyword>");
                            ImGui::BulletText("%s", keywordLabel);
                            const std::string keywordPopupID = "KeywordCopyPopup##" + std::to_string(displayed);
                            if (ImGui::BeginPopupContextItem(keywordPopupID.c_str())) {
                                if (ImGui::MenuItem(context.localize("General", "sCopy", "Copy"))) {
                                    ImGui::SetClipboardText(keywordLabel);
                                }
                                ImGui::EndPopup();
                            }
                            ++displayed;
                            return RE::BSContainer::ForEachResult::kContinue;
                        });
                        if (displayed == 0) {
                            ImGui::TextDisabled("%s", context.localize("General", "sNoKeywords", "<No keywords>"));
                        }
                    }
                }
                }
                ImGui::EndChild();

                const bool canGive = CanGiveFromTreeCategory(selectedRecord->category);
                const bool canSpawn = CanSpawnFromTreeCategory(selectedRecord->category);
                const bool canTeleport = CanTeleportFromTreeCategory(selectedRecord->category);
                const bool isQuest = IsQuestCategory(selectedRecord->category);
                const bool isPerk = IsPerkCategory(selectedRecord->category);
                const bool isSpellLike = IsSpellLikeCategory(selectedRecord->category);
                const bool isWeather = IsWeatherCategory(selectedRecord->category);
                const bool isSound = IsSoundCategory(selectedRecord->category);
                const bool isGlobal = IsGlobalCategory(selectedRecord->category);
                const bool isOutfit = IsOutfitCategory(selectedRecord->category);
                const bool isConstructible = IsConstructibleCategory(selectedRecord->category);
                const bool isEquippable = selectedRecord->category == "WEAP" || selectedRecord->category == "Weapon" || selectedRecord->category == "ARMO" || selectedRecord->category == "Armor";

                auto wrappedButton = [](const char* label, bool& firstInRow) {
                    const auto& style = ImGui::GetStyle();
                    const float buttonWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;

                    if (!firstInRow) {
                        const float needed = style.ItemSpacing.x + buttonWidth;
                        if (ImGui::GetContentRegionAvail().x >= needed) {
                            ImGui::SameLine();
                        }
                    }

                    const bool pressed = ImGui::Button(label);
                    firstInRow = false;
                    return pressed;
                };

                bool firstBtn = true;

                const bool isFavorite = context.favoriteForms.contains(selectedRecord->formID);
                if (wrappedButton(isFavorite ? context.localize("General", "sRemoveFavorite", "Remove Favorite") : context.localize("General", "sAddFavorite", "Add Favorite"), firstBtn)) {
                    if (isFavorite) {
                        context.favoriteForms.erase(selectedRecord->formID);
                    } else {
                        context.favoriteForms.insert(selectedRecord->formID);
                    }
                }

                if (wrappedButton(context.localize("General", "sCopyFormID", "Copy FormID"), firstBtn)) {
                    FormActions::CopyFormID(selectedRecord->formID);
                }

                ImGui::Separator();

                firstBtn = true;
                if (hasMultipleSelection && !selectedGiveableEntries.empty()) {
                    std::string giveSelectedLabel = std::string(context.localize("Items", "sGiveItem", "Give Item")) + " (" + std::to_string(selectedGiveableEntries.size()) + ")";
                    if (wrappedButton(giveSelectedLabel.c_str(), firstBtn)) {
                        context.openItemGrantPopupMultiple(selectedGiveableEntries);
                    }
                }

                if (canGive) {
                    if (wrappedButton(context.localize("Items", "sGiveItem", "Give Item"), firstBtn)) {
                        context.openItemGrantPopup(*selectedRecord);
                    }
                }

                if (canSpawn || canGive) {
                    if (!firstBtn) {
                        const auto& style = ImGui::GetStyle();
                        const float spawnBtnWidth = ImGui::CalcTextSize(context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player")).x + style.FramePadding.x * 2.0f;
                        const float needed = style.ItemSpacing.x + 80.0f + style.ItemSpacing.x + spawnBtnWidth;
                        if (ImGui::GetContentRegionAvail().x >= needed) {
                            ImGui::SameLine();
                        }
                    }
                    static int detailSpawnQuantity = 1;
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::InputInt("##DetailSpawnQty", &detailSpawnQuantity, 1, 10);
                    if (detailSpawnQuantity < 1) {
                        detailSpawnQuantity = 1;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                        const auto formID = selectedRecord->formID;
                        const auto quantity = static_cast<std::uint32_t>(detailSpawnQuantity);
                        const std::string name = selectedRecord->name;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmSpawnTitle", "Confirm Spawn"),
                            std::string(context.localize("General", "sConfirmSpawnMessage", "Spawn selected record at player?")) + "\n" + (name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : name),
                            [formID, quantity]() {
                                FormActions::SpawnAtPlayer(formID, quantity);
                            });
                    }
                    firstBtn = false;
                }

                if (isQuest) {
                    if (wrappedButton(context.localize("General", "sStartQuest", "Start Quest"), firstBtn)) {
                        const auto formID = selectedRecord->formID;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmQuestTitle", "Confirm Quest Action"),
                            context.localize("General", "sConfirmStartQuest", "Start selected quest?"),
                            [formID]() {
                                char command[64]{};
                                std::snprintf(command, sizeof(command), "startquest %08X", formID);
                                FormActions::ExecuteConsoleCommand(command);
                            });
                    }
                    if (wrappedButton(context.localize("General", "sCompleteQuest", "Complete Quest"), firstBtn)) {
                        const auto formID = selectedRecord->formID;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmQuestTitle", "Confirm Quest Action"),
                            context.localize("General", "sConfirmCompleteQuest", "Complete selected quest?"),
                            [formID]() {
                                char command[64]{};
                                std::snprintf(command, sizeof(command), "completequest %08X", formID);
                                FormActions::ExecuteConsoleCommand(command);
                            });
                    }
                }

                if (isPerk) {
                    if (wrappedButton(context.localize("General", "sAddPerk", "Add Perk"), firstBtn)) {
                        FormActions::AddPerkToPlayer(selectedRecord->formID);
                    }
                    if (wrappedButton(context.localize("General", "sRemovePerk", "Remove Perk"), firstBtn)) {
                        FormActions::RemovePerkFromPlayer(selectedRecord->formID);
                    }
                }

                if (isSpellLike) {
                    if (wrappedButton(context.localize("General", "sAddSpellEffect", "Add Spell/Effect"), firstBtn)) {
                        FormActions::AddSpellToPlayer(selectedRecord->formID);
                    }
                    if (wrappedButton(context.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"), firstBtn)) {
                        FormActions::RemoveSpellFromPlayer(selectedRecord->formID);
                    }
                }

                if (isWeather) {
                    if (wrappedButton(context.localize("General", "sSetWeather", "Set Weather"), firstBtn)) {
                        const auto formID = selectedRecord->formID;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmWeatherTitle", "Confirm Weather Change"),
                            context.localize("General", "sConfirmWeather", "Set current weather to selected weather record?"),
                            [formID]() {
                                char command[64]{};
                                std::snprintf(command, sizeof(command), "fw %08X", formID);
                                FormActions::ExecuteConsoleCommand(command);
                            });
                    }
                }

                if (isSound) {
                    if (wrappedButton(context.localize("General", "sPlaySound", "Play Sound"), firstBtn)) {
                        if (const char* editorID = TryGetEditorID(selectedRecord->formID)) {
                            std::string command = std::string("playsound ") + editorID;
                            FormActions::ExecuteConsoleCommand(command);
                        }
                    }
                }

                if (isGlobal) {
                    if (wrappedButton(context.localize("General", "sSetGlobal", "Set Global"), firstBtn)) {
                        context.openGlobalValuePopup(selectedRecord->formID);
                    }
                }

                if (isOutfit) {
                    if (wrappedButton(context.localize("General", "sAddOutfitItems", "Add Outfit Items"), firstBtn)) {
                        FormActions::AddOutfitItemsToPlayer(selectedRecord->formID);
                    }
                }

                if (isConstructible) {
                    if (wrappedButton(context.localize("General", "sAddCraftedItem", "Add Crafted Item"), firstBtn)) {
                        FormActions::AddConstructedItemToPlayer(selectedRecord->formID);
                    }
                }

                if (isEquippable) {
                    if (wrappedButton(context.localize("General", "sEquipItem", "Equip Item"), firstBtn)) {
                        EquipRecordWithConfiguredAmmo(*selectedRecord, context.equipWeaponAmmoCount);
                    }
                }

                if (canTeleport) {
                    auto* teleportForm = RE::TESForm::GetFormByID(selectedRecord->formID);
                    const char* editorID = teleportForm ? teleportForm->GetFormEditorID() : nullptr;
                    const bool canUseCoc = editorID && editorID[0] != '\0';

                    if (canUseCoc) {
                        if (wrappedButton(context.localize("General", "sTeleportCOC", "Teleport (COC)"), firstBtn)) {
                            const auto editorIDCopy = std::string(editorID);
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmTeleportTitle", "Confirm Teleport"),
                                context.localize("General", "sConfirmTeleport", "Teleport to selected destination?"),
                                [editorIDCopy]() {
                                    std::string command = std::string("coc ") + editorIDCopy;
                                    FormActions::ExecuteConsoleCommand(command);
                                });
                        }
                    } else {
                        ImGui::BeginDisabled(true);
                        ImGui::Button(context.localize("General", "sTeleportCOC", "Teleport (COC)"));
                        ImGui::EndDisabled();
                    }
                }
            }
        }
        ImGui::EndChild();
    }
}
