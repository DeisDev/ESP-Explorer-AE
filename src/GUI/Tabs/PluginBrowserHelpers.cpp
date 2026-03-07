#include "GUI/Tabs/PluginBrowserHelpers.h"

#include "Config/Config.h"

#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/SharedUtils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace ESPExplorerAE::PluginBrowserHelpers
{
    namespace
    {
        std::unordered_map<std::uint32_t, std::string> editorIdCache{};

        char FoldCase(unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        }

        std::string_view GetEditorID(std::uint32_t formID)
        {
            auto [it, inserted] = editorIdCache.try_emplace(formID);
            if (inserted) {
                if (const char* editorID = ContextMenu::TryGetEditorID(formID)) {
                    it->second = editorID;
                }
            }

            return it->second;
        }

        bool EqualsCaseInsensitive(std::string_view left, std::string_view right)
        {
            return left.size() == right.size() &&
                   std::equal(left.begin(), left.end(), right.begin(), [](char lhs, char rhs) {
                       return FoldCase(static_cast<unsigned char>(lhs)) == FoldCase(static_cast<unsigned char>(rhs));
                   });
        }

        bool IsDataCategory(std::string_view category)
        {
            return category == "KYWD" || category == "FLST" || category == "GLOB" || category == "COBJ";
        }

        const PluginInfo* FindPluginInfo(std::string_view pluginName, const std::vector<PluginInfo>& plugins)
        {
            const auto it = std::ranges::find_if(plugins, [pluginName](const PluginInfo& plugin) {
                return plugin.filename == pluginName;
            });
            return it != plugins.end() ? &(*it) : nullptr;
        }

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
    }

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

    bool MatchesPluginSearch(const FormEntry& entry, std::string_view query, bool caseSensitive)
    {
        if (query.empty()) {
            return true;
        }

        char formIDBuffer[16]{};
        std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);

        if (SharedUtils::ContainsByMode(entry.name, query, caseSensitive) ||
            SharedUtils::ContainsByMode(entry.category, query, caseSensitive) ||
            SharedUtils::ContainsByMode(entry.sourcePlugin, query, caseSensitive) ||
            SharedUtils::ContainsByMode(formIDBuffer, query, caseSensitive)) {
            return true;
        }

        const auto editorID = GetEditorID(entry.formID);
        return !editorID.empty() && SharedUtils::ContainsByMode(editorID, query, caseSensitive);
    }

    bool IsUnknownCategory(std::string_view category)
    {
        if (category.empty()) {
            return true;
        }

        return EqualsCaseInsensitive(category, "unknown") || EqualsCaseInsensitive(category, "<unknown>");
    }

    std::string BuildPluginDisplayName(std::string_view pluginName, const std::vector<PluginInfo>& plugins)
    {
        if (const auto* plugin = FindPluginInfo(pluginName, plugins); plugin && !plugin->formIDPrefix.empty()) {
            return std::string(pluginName) + " [" + plugin->formIDPrefix + "]";
        }

        return std::string(pluginName);
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
        if (ContextMenu::CanGiveItem(category)) {
            return ImVec4(0.40f, 0.80f, 0.40f, 1.00f);
        }
        if (ContextMenu::CanSpawn(category)) {
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

        const std::size_t maxRecentRecords = static_cast<std::size_t>((std::clamp)(Config::Get().recentRecordsLimit, 5, 100));

        context.recentPluginRecordFormIDs.erase(
            std::remove(context.recentPluginRecordFormIDs.begin(), context.recentPluginRecordFormIDs.end(), formID),
            context.recentPluginRecordFormIDs.end());
        context.recentPluginRecordFormIDs.push_front(formID);

        while (context.recentPluginRecordFormIDs.size() > maxRecentRecords) {
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
            if (!selectedEntry || !ContextMenu::CanGiveItem(selectedEntry->category)) {
                continue;
            }

            selectedEntries.push_back(*selectedEntry);
        }

        return selectedEntries;
    }

    std::vector<FormEntry> CollectSelectedEntries(const FormCache& cache, std::uint64_t dataVersion, const PluginBrowserTabContext& context)
    {
        std::vector<FormEntry> selectedEntries{};
        selectedEntries.reserve(context.selectedPluginTreeRecordFormIDs.size());

        for (const auto selectedFormID : context.selectedPluginTreeRecordFormIDs) {
            const auto* selectedEntry = FindRecordByFormID(cache, selectedFormID, dataVersion);
            if (!selectedEntry) {
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

    void DrawRecordContextMenu(const FormEntry& record, bool isSelected, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
        if (ImGui::MenuItem(isSelected ? context.localize("General", "sDeselect", "Deselect") : context.localize("General", "sSelect", "Select"))) {
            if (isSelected) {
                context.selectedPluginTreeRecordFormIDs.erase(record.formID);
            } else {
                context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                context.selectedPluginTreeRecordFormID = record.formID;
                context.pluginTreeLastClickedFormID = record.formID;
            }
            EnsurePrimarySelectionValid(context);
        }

        ImGui::Separator();

        const bool isMultiSelectedRecord = isSelected && context.selectedPluginTreeRecordFormIDs.size() > 1;
        if (isMultiSelectedRecord) {
            const auto selectedGiveableEntries = CollectSelectedGiveableEntries(cache, dataVersion, context);
            if (!selectedGiveableEntries.empty()) {
                std::string grantLabel = std::string(context.localize("Items", "sGiveItem", "Give Item")) + " (" + std::to_string(selectedGiveableEntries.size()) + ")";
                if (ImGui::MenuItem(grantLabel.c_str())) {
                    context.openItemGrantPopupMultiple(selectedGiveableEntries);
                }
            }

            const auto selectedEntries = CollectSelectedEntries(cache, dataVersion, context);
            if (!selectedEntries.empty()) {
                if (ImGui::MenuItem((std::string(context.localize("General", "sCopyFormID", "Copy FormID")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                    std::vector<std::string> values{};
                    values.reserve(selectedEntries.size());
                    for (const auto& selectedEntry : selectedEntries) {
                        char idBuffer[16]{};
                        std::snprintf(idBuffer, sizeof(idBuffer), "%08X", selectedEntry.formID);
                        values.emplace_back(idBuffer);
                    }
                    const auto text = SharedUtils::BuildParenthesizedList(values);
                    ImGui::SetClipboardText(text.c_str());
                }

                if (ImGui::MenuItem((std::string(context.localize("General", "sCopyRecordSource", "Copy Record Source")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                    std::vector<std::string> values{};
                    values.reserve(selectedEntries.size());
                    for (const auto& selectedEntry : selectedEntries) {
                        values.push_back(selectedEntry.sourcePlugin);
                    }
                    const auto text = SharedUtils::BuildParenthesizedList(values);
                    ImGui::SetClipboardText(text.c_str());
                }

                if (ImGui::MenuItem((std::string(context.localize("General", "sCopyName", "Copy Name")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                    std::vector<std::string> values{};
                    values.reserve(selectedEntries.size());
                    for (const auto& selectedEntry : selectedEntries) {
                        values.push_back(selectedEntry.name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                    }
                    const auto text = SharedUtils::BuildParenthesizedList(values);
                    ImGui::SetClipboardText(text.c_str());
                }

                if (ImGui::MenuItem((std::string(context.localize("General", "sAddFavorite", "Add Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                    for (const auto& selectedEntry : selectedEntries) {
                        context.favoriteForms.insert(selectedEntry.formID);
                    }
                }

                if (ImGui::MenuItem((std::string(context.localize("General", "sRemoveFavorite", "Remove Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                    for (const auto& selectedEntry : selectedEntries) {
                        context.favoriteForms.erase(selectedEntry.formID);
                    }
                }
            }

            ImGui::Separator();
        }

        auto callbacks = BuildContextCallbacks(context);
        callbacks.hideCopyAndFavoriteActions = isMultiSelectedRecord;
        ContextMenu::Draw(record, callbacks);
    }

    void RebuildCachesIfNeeded(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
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

        if (!needsCacheRebuild) {
            return;
        }

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
}