#include "GUI/Tabs/PluginBrowserTab.h"

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

        const FormEntry* FindRecordByFormID(const FormCache& cache, std::uint32_t formID)
        {
            for (const auto& entry : cache.allRecords) {
                if (entry.formID == formID) {
                    return &entry;
                }
            }

            return nullptr;
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
    }

    void PluginBrowserTab::Draw(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
        bool listFilterSettingsChanged = false;

        if (ImGui::InputText(context.localize("PluginBrowser", "sSearch", "Plugin Search"), context.pluginSearchBuffer, context.pluginSearchBufferSize)) {
            context.pluginSearch = context.pluginSearchBuffer;
        }
        ImGui::SameLine();

        if (ImGui::Button(context.localize("PluginBrowser", "sClearFilter", "Clear Plugin Filter"))) {
            context.selectedPluginFilter.clear();
        }

        if (!context.selectedPluginFilter.empty()) {
            ImGui::SameLine();
            ImGui::Text("%s: %s", context.localize("PluginBrowser", "sActiveFilter", "Active"), context.selectedPluginFilter.c_str());
        }

        if (RecordFiltersWidget::Draw(
                context.localize,
                "PluginBrowser",
                RecordFilterState{
                    .showPlayable = context.showPlayableRecords,
                    .showNonPlayable = context.showNonPlayableRecords,
                    .showNamed = context.showNamedRecords,
                    .showUnnamed = context.showUnnamedRecords,
                    .showDeleted = context.showDeletedRecords })) {
            listFilterSettingsChanged = true;
        }

        if (ImGui::Checkbox(context.localize("PluginBrowser", "sShowUnknownCategories", "Show Unknown Categories"), &context.showUnknownCategories)) {
            listFilterSettingsChanged = true;
        }

        ImGui::SameLine();
        ImGui::Checkbox(context.localize("PluginBrowser", "sGlobalSearch", "Global Search"), &context.pluginGlobalSearchMode);
        ImGui::SameLine();
        ImGui::Checkbox(context.localize("PluginBrowser", "sCaseSensitiveSearch", "Case Sensitive"), &context.pluginSearchCaseSensitive);

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
            context.pluginBrowserCacheGlobalSearchMode != context.pluginGlobalSearchMode ||
            context.pluginBrowserCacheSearchCaseSensitive != context.pluginSearchCaseSensitive;

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

                const bool searchMatches = MatchesPluginSearch(entry, context.pluginSearch, context.pluginSearchCaseSensitive);

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
            context.pluginBrowserCacheSearchCaseSensitive = context.pluginSearchCaseSensitive;
        }

        const float leftWidth = ImGui::GetContentRegionAvail().x * 0.58f;

        if (ImGui::BeginChild("PluginTreeLeft", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders)) {
            auto drawRecordSelectable = [&](const FormEntry& record, std::string_view idPrefix) {
                const auto* displayName = record.name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : record.name.c_str();
                char recordLabel[512]{};
                std::snprintf(recordLabel, sizeof(recordLabel), "%s [%08X]##%s%08X", displayName, record.formID, std::string(idPrefix).c_str(), record.formID);
                const bool isSelected = context.selectedPluginTreeRecordFormID == record.formID;
                if (ImGui::Selectable(recordLabel, isSelected)) {
                    context.selectedPluginTreeRecordFormID = record.formID;
                    TrackRecentRecord(record.formID, context);
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (!isSelected) {
                        context.selectedPluginTreeRecordFormID = record.formID;
                        TrackRecentRecord(record.formID, context);
                    }

                    if (ImGui::MenuItem(context.localize("General", "sCopyFormID", "Copy FormID"))) {
                        FormActions::CopyFormID(record.formID);
                    }

                    if (const char* editorID = TryGetEditorID(record.formID); editorID && editorID[0] != '\0') {
                        if (ImGui::MenuItem(context.localize("General", "sCopyEditorID", "Copy EditorID"))) {
                            ImGui::SetClipboardText(editorID);
                        }
                    }

                    const auto sourceText = record.sourcePlugin.empty() ? context.localize("General", "sUnknown", "<Unknown>") : record.sourcePlugin.c_str();
                    if (ImGui::MenuItem(context.localize("General", "sCopyRecordSource", "Copy Record Source"))) {
                        ImGui::SetClipboardText(sourceText);
                    }

                    ImGui::Separator();

                    const bool isFavorite = context.favoriteForms.contains(record.formID);
                    if (ImGui::MenuItem(isFavorite ? context.localize("General", "sRemoveFavorite", "Remove Favorite") : context.localize("General", "sAddFavorite", "Add Favorite"))) {
                        if (isFavorite) {
                            context.favoriteForms.erase(record.formID);
                        } else {
                            context.favoriteForms.insert(record.formID);
                        }
                    }

                    if (CanGiveFromTreeCategory(record.category) && ImGui::MenuItem(context.localize("Items", "sGiveItem", "Give Item"))) {
                        context.openItemGrantPopup(record);
                    }

                    if (CanSpawnFromTreeCategory(record.category) && ImGui::MenuItem(context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                        const auto formID = record.formID;
                        const std::string name = record.name;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmSpawnTitle", "Confirm Spawn"),
                            std::string(context.localize("General", "sConfirmSpawnMessage", "Spawn selected record at player?")) + "\n" + (name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : name),
                            [formID]() {
                                FormActions::SpawnAtPlayer(formID, 1);
                            });
                    }

                    if (IsQuestCategory(record.category)) {
                        if (ImGui::MenuItem(context.localize("General", "sStartQuest", "Start Quest"))) {
                            const auto formID = record.formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmQuestTitle", "Confirm Quest Action"),
                                context.localize("General", "sConfirmStartQuest", "Start selected quest?"),
                                [formID]() {
                                    char command[64]{};
                                    std::snprintf(command, sizeof(command), "startquest %08X", formID);
                                    FormActions::ExecuteConsoleCommand(command);
                                });
                        }
                        if (ImGui::MenuItem(context.localize("General", "sCompleteQuest", "Complete Quest"))) {
                            const auto formID = record.formID;
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

                    if (IsPerkCategory(record.category)) {
                        if (ImGui::MenuItem(context.localize("General", "sAddPerk", "Add Perk"))) {
                            FormActions::AddPerkToPlayer(record.formID);
                        }
                        if (ImGui::MenuItem(context.localize("General", "sRemovePerk", "Remove Perk"))) {
                            FormActions::RemovePerkFromPlayer(record.formID);
                        }
                    }

                    if (IsSpellLikeCategory(record.category)) {
                        if (ImGui::MenuItem(context.localize("General", "sAddSpellEffect", "Add Spell/Effect"))) {
                            FormActions::AddSpellToPlayer(record.formID);
                        }
                        if (ImGui::MenuItem(context.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"))) {
                            FormActions::RemoveSpellFromPlayer(record.formID);
                        }
                    }

                    if (IsWeatherCategory(record.category) && ImGui::MenuItem(context.localize("General", "sSetWeather", "Set Weather"))) {
                        const auto formID = record.formID;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmWeatherTitle", "Confirm Weather Change"),
                            context.localize("General", "sConfirmWeather", "Set current weather to selected weather record?"),
                            [formID]() {
                                char command[64]{};
                                std::snprintf(command, sizeof(command), "fw %08X", formID);
                                FormActions::ExecuteConsoleCommand(command);
                            });
                    }

                    if (IsSoundCategory(record.category) && ImGui::MenuItem(context.localize("General", "sPlaySound", "Play Sound"))) {
                        if (const char* editorID = TryGetEditorID(record.formID)) {
                            std::string command = std::string("playsound ") + editorID;
                            FormActions::ExecuteConsoleCommand(command);
                        }
                    }

                    if (IsGlobalCategory(record.category) && ImGui::MenuItem(context.localize("General", "sSetGlobal", "Set Global"))) {
                        context.openGlobalValuePopup(record.formID);
                    }

                    if (IsOutfitCategory(record.category) && ImGui::MenuItem(context.localize("General", "sAddOutfitItems", "Add Outfit Items"))) {
                        FormActions::AddOutfitItemsToPlayer(record.formID);
                    }

                    if (IsConstructibleCategory(record.category) && ImGui::MenuItem(context.localize("General", "sAddCraftedItem", "Add Crafted Item"))) {
                        FormActions::AddConstructedItemToPlayer(record.formID);
                    }

                    const bool isEquippable = record.category == "WEAP" || record.category == "Weapon" || record.category == "ARMO" || record.category == "Armor";
                    if (isEquippable && ImGui::MenuItem(context.localize("General", "sEquipItem", "Equip Item"))) {
                        char command[64]{};
                        std::snprintf(command, sizeof(command), "player.equipitem %08X", record.formID);
                        FormActions::ExecuteConsoleCommand(command);
                    }

                    if (CanTeleportFromTreeCategory(record.category)) {
                        auto* teleportForm = RE::TESForm::GetFormByID(record.formID);
                        const char* editorID = teleportForm ? teleportForm->GetFormEditorID() : nullptr;
                        const bool canUseCoc = editorID && editorID[0] != '\0';
                        if (canUseCoc && ImGui::MenuItem(context.localize("General", "sTeleportCOC", "Teleport (COC)"))) {
                            const auto editorIDCopy = std::string(editorID);
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmTeleportTitle", "Confirm Teleport"),
                                context.localize("General", "sConfirmTeleport", "Teleport to selected destination?"),
                                [editorIDCopy]() {
                                    std::string command = std::string("coc ") + editorIDCopy;
                                    FormActions::ExecuteConsoleCommand(command);
                                });
                        }
                    }

                    ImGui::EndPopup();
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && CanGiveFromTreeCategory(record.category)) {
                    context.openItemGrantPopup(record);
                }
            };

            const std::string globalResultsHeader = std::string(context.localize("PluginBrowser", "sGlobalSearchResults", "Global Search Results")) + "##PluginGlobalSearchResults";
            if (context.pluginGlobalSearchMode && !context.pluginSearch.empty() && ImGui::TreeNodeEx(globalResultsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                for (const auto* record : context.pluginBrowserGlobalSearchResultsCache) {
                    if (!record) {
                        continue;
                    }
                    drawRecordSelectable(*record, "GlobalResult");
                }
                ImGui::TreePop();
            }

            const std::string favoritesHeader = std::string(context.localize("General", "sFavorites", "Favorites")) + "##PluginFavorites";
            if (ImGui::TreeNodeEx(favoritesHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                for (const auto& entry : cache.allRecords) {
                    if (!context.favoriteForms.contains(entry.formID)) {
                        continue;
                    }
                    if (!PassesLocalRecordFilters(entry, context)) {
                        continue;
                    }
                    if (!context.showUnknownCategories && (entry.sourcePlugin.empty() || IsUnknownCategory(entry.category))) {
                        continue;
                    }
                    if (!context.pluginSearch.empty() && !MatchesPluginSearch(entry, context.pluginSearch, context.pluginSearchCaseSensitive)) {
                        continue;
                    }

                    drawRecordSelectable(entry, "FavoriteRecord");
                }
                ImGui::TreePop();
            }

            const std::string recentRecordsHeader = std::string(context.localize("PluginBrowser", "sRecentRecords", "Recent Records")) + "##PluginRecentRecords";
            if (ImGui::TreeNodeEx(recentRecordsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                for (const auto recentFormID : context.recentPluginRecordFormIDs) {
                    const auto* recentEntry = FindRecordByFormID(cache, recentFormID);
                    if (!recentEntry) {
                        continue;
                    }
                    if (!PassesLocalRecordFilters(*recentEntry, context)) {
                        continue;
                    }
                    if (!context.showUnknownCategories && (recentEntry->sourcePlugin.empty() || IsUnknownCategory(recentEntry->category))) {
                        continue;
                    }
                    if (!context.pluginSearch.empty() && !MatchesPluginSearch(*recentEntry, context.pluginSearch, context.pluginSearchCaseSensitive)) {
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
                            for (const auto* record : categoryIt->second) {
                                if (!record) {
                                    continue;
                                }

                                const auto* displayName = record->name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : record->name.c_str();
                                char recordLabel[512]{};
                                std::snprintf(recordLabel, sizeof(recordLabel), "%s [%08X]##TreeRecord%08X", displayName, record->formID, record->formID);

                                const bool isSelected = context.selectedPluginTreeRecordFormID == record->formID;
                                if (ImGui::Selectable(recordLabel, isSelected)) {
                                    context.selectedPluginTreeRecordFormID = record->formID;
                                    TrackRecentRecord(record->formID, context);
                                }
                                if (ImGui::BeginPopupContextItem()) {
                                    if (!isSelected) {
                                        context.selectedPluginTreeRecordFormID = record->formID;
                                        TrackRecentRecord(record->formID, context);
                                    }

                                    if (ImGui::MenuItem(context.localize("General", "sCopyFormID", "Copy FormID"))) {
                                        FormActions::CopyFormID(record->formID);
                                    }

                                    if (const char* editorID = TryGetEditorID(record->formID); editorID && editorID[0] != '\0') {
                                        if (ImGui::MenuItem(context.localize("General", "sCopyEditorID", "Copy EditorID"))) {
                                            ImGui::SetClipboardText(editorID);
                                        }
                                    }

                                    const auto sourceText = record->sourcePlugin.empty() ? context.localize("General", "sUnknown", "<Unknown>") : record->sourcePlugin.c_str();
                                    if (ImGui::MenuItem(context.localize("General", "sCopyRecordSource", "Copy Record Source"))) {
                                        ImGui::SetClipboardText(sourceText);
                                    }

                                    ImGui::Separator();

                                    const bool isFavorite = context.favoriteForms.contains(record->formID);
                                    if (ImGui::MenuItem(isFavorite ? context.localize("General", "sRemoveFavorite", "Remove Favorite") : context.localize("General", "sAddFavorite", "Add Favorite"))) {
                                        if (isFavorite) {
                                            context.favoriteForms.erase(record->formID);
                                        } else {
                                            context.favoriteForms.insert(record->formID);
                                        }
                                    }

                                    if (CanGiveFromTreeCategory(record->category) && ImGui::MenuItem(context.localize("Items", "sGiveItem", "Give Item"))) {
                                        context.openItemGrantPopup(*record);
                                    }

                                    if (CanSpawnFromTreeCategory(record->category) && ImGui::MenuItem(context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                                        const auto formID = record->formID;
                                        const std::string name = record->name;
                                        context.requestActionConfirmation(
                                            context.localize("General", "sConfirmSpawnTitle", "Confirm Spawn"),
                                            std::string(context.localize("General", "sConfirmSpawnMessage", "Spawn selected record at player?")) + "\n" + (name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : name),
                                            [formID]() {
                                                FormActions::SpawnAtPlayer(formID, 1);
                                            });
                                    }

                                    if (IsQuestCategory(record->category)) {
                                        if (ImGui::MenuItem(context.localize("General", "sStartQuest", "Start Quest"))) {
                                            const auto formID = record->formID;
                                            context.requestActionConfirmation(
                                                context.localize("General", "sConfirmQuestTitle", "Confirm Quest Action"),
                                                context.localize("General", "sConfirmStartQuest", "Start selected quest?"),
                                                [formID]() {
                                                    char command[64]{};
                                                    std::snprintf(command, sizeof(command), "startquest %08X", formID);
                                                    FormActions::ExecuteConsoleCommand(command);
                                                });
                                        }
                                        if (ImGui::MenuItem(context.localize("General", "sCompleteQuest", "Complete Quest"))) {
                                            const auto formID = record->formID;
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

                                    if (IsPerkCategory(record->category)) {
                                        if (ImGui::MenuItem(context.localize("General", "sAddPerk", "Add Perk"))) {
                                            FormActions::AddPerkToPlayer(record->formID);
                                        }
                                        if (ImGui::MenuItem(context.localize("General", "sRemovePerk", "Remove Perk"))) {
                                            FormActions::RemovePerkFromPlayer(record->formID);
                                        }
                                    }

                                    if (IsSpellLikeCategory(record->category)) {
                                        if (ImGui::MenuItem(context.localize("General", "sAddSpellEffect", "Add Spell/Effect"))) {
                                            FormActions::AddSpellToPlayer(record->formID);
                                        }
                                        if (ImGui::MenuItem(context.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"))) {
                                            FormActions::RemoveSpellFromPlayer(record->formID);
                                        }
                                    }

                                    if (IsWeatherCategory(record->category) && ImGui::MenuItem(context.localize("General", "sSetWeather", "Set Weather"))) {
                                        const auto formID = record->formID;
                                        context.requestActionConfirmation(
                                            context.localize("General", "sConfirmWeatherTitle", "Confirm Weather Change"),
                                            context.localize("General", "sConfirmWeather", "Set current weather to selected weather record?"),
                                            [formID]() {
                                                char command[64]{};
                                                std::snprintf(command, sizeof(command), "fw %08X", formID);
                                                FormActions::ExecuteConsoleCommand(command);
                                            });
                                    }

                                    if (IsSoundCategory(record->category) && ImGui::MenuItem(context.localize("General", "sPlaySound", "Play Sound"))) {
                                        if (const char* editorID = TryGetEditorID(record->formID)) {
                                            std::string command = std::string("playsound ") + editorID;
                                            FormActions::ExecuteConsoleCommand(command);
                                        }
                                    }

                                    if (IsGlobalCategory(record->category) && ImGui::MenuItem(context.localize("General", "sSetGlobal", "Set Global"))) {
                                        context.openGlobalValuePopup(record->formID);
                                    }

                                    if (IsOutfitCategory(record->category) && ImGui::MenuItem(context.localize("General", "sAddOutfitItems", "Add Outfit Items"))) {
                                        FormActions::AddOutfitItemsToPlayer(record->formID);
                                    }

                                    if (IsConstructibleCategory(record->category) && ImGui::MenuItem(context.localize("General", "sAddCraftedItem", "Add Crafted Item"))) {
                                        FormActions::AddConstructedItemToPlayer(record->formID);
                                    }

                                    const bool isEquippable = record->category == "WEAP" || record->category == "Weapon" || record->category == "ARMO" || record->category == "Armor";
                                    if (isEquippable && ImGui::MenuItem(context.localize("General", "sEquipItem", "Equip Item"))) {
                                        char command[64]{};
                                        std::snprintf(command, sizeof(command), "player.equipitem %08X", record->formID);
                                        FormActions::ExecuteConsoleCommand(command);
                                    }

                                    if (CanTeleportFromTreeCategory(record->category)) {
                                        auto* teleportForm = RE::TESForm::GetFormByID(record->formID);
                                        const char* editorID = teleportForm ? teleportForm->GetFormEditorID() : nullptr;
                                        const bool canUseCoc = editorID && editorID[0] != '\0';
                                        if (canUseCoc && ImGui::MenuItem(context.localize("General", "sTeleportCOC", "Teleport (COC)"))) {
                                            const auto editorIDCopy = std::string(editorID);
                                            context.requestActionConfirmation(
                                                context.localize("General", "sConfirmTeleportTitle", "Confirm Teleport"),
                                                context.localize("General", "sConfirmTeleport", "Teleport to selected destination?"),
                                                [editorIDCopy]() {
                                                    std::string command = std::string("coc ") + editorIDCopy;
                                                    FormActions::ExecuteConsoleCommand(command);
                                                });
                                        }
                                    }

                                    ImGui::EndPopup();
                                }

                                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && CanGiveFromTreeCategory(record->category)) {
                                    context.openItemGrantPopup(*record);
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
            const FormEntry* selectedRecord = nullptr;
            if (context.selectedPluginTreeRecordFormID != 0) {
                for (const auto& entry : cache.allRecords) {
                    if (entry.formID == context.selectedPluginTreeRecordFormID) {
                        selectedRecord = &entry;
                        break;
                    }
                }
            }

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

                ImGui::Separator();
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

                const bool isFavorite = context.favoriteForms.contains(selectedRecord->formID);
                if (ImGui::Button(isFavorite ? context.localize("General", "sRemoveFavorite", "Remove Favorite") : context.localize("General", "sAddFavorite", "Add Favorite"))) {
                    if (isFavorite) {
                        context.favoriteForms.erase(selectedRecord->formID);
                    } else {
                        context.favoriteForms.insert(selectedRecord->formID);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(context.localize("General", "sCopyFormID", "Copy FormID"))) {
                    FormActions::CopyFormID(selectedRecord->formID);
                }

                ImGui::Separator();

                bool hasActionOnRow = false;
                if (CanGiveFromTreeCategory(selectedRecord->category)) {
                    if (ImGui::Button(context.localize("Items", "sGiveItem", "Give Item"))) {
                        context.openItemGrantPopup(*selectedRecord);
                    }
                    hasActionOnRow = true;
                }

                if (canSpawn) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                        const auto formID = selectedRecord->formID;
                        const std::string name = selectedRecord->name;
                        context.requestActionConfirmation(
                            context.localize("General", "sConfirmSpawnTitle", "Confirm Spawn"),
                            std::string(context.localize("General", "sConfirmSpawnMessage", "Spawn selected record at player?")) + "\n" + (name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : name),
                            [formID]() {
                                FormActions::SpawnAtPlayer(formID, 1);
                            });
                    }
                    hasActionOnRow = true;
                }

                if (isQuest) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sStartQuest", "Start Quest"))) {
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
                    ImGui::SameLine();
                    if (ImGui::Button(context.localize("General", "sCompleteQuest", "Complete Quest"))) {
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
                    hasActionOnRow = true;
                }

                if (isPerk) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sAddPerk", "Add Perk"))) {
                        FormActions::AddPerkToPlayer(selectedRecord->formID);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(context.localize("General", "sRemovePerk", "Remove Perk"))) {
                        FormActions::RemovePerkFromPlayer(selectedRecord->formID);
                    }
                    hasActionOnRow = true;
                }

                if (isSpellLike) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sAddSpellEffect", "Add Spell/Effect"))) {
                        FormActions::AddSpellToPlayer(selectedRecord->formID);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(context.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"))) {
                        FormActions::RemoveSpellFromPlayer(selectedRecord->formID);
                    }
                    hasActionOnRow = true;
                }

                if (isWeather) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sSetWeather", "Set Weather"))) {
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
                    hasActionOnRow = true;
                }

                if (isSound) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sPlaySound", "Play Sound"))) {
                        if (const char* editorID = TryGetEditorID(selectedRecord->formID)) {
                            std::string command = std::string("playsound ") + editorID;
                            FormActions::ExecuteConsoleCommand(command);
                        }
                    }
                    hasActionOnRow = true;
                }

                if (isGlobal) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sSetGlobal", "Set Global"))) {
                        context.openGlobalValuePopup(selectedRecord->formID);
                    }
                    hasActionOnRow = true;
                }

                if (isOutfit) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sAddOutfitItems", "Add Outfit Items"))) {
                        FormActions::AddOutfitItemsToPlayer(selectedRecord->formID);
                    }
                    hasActionOnRow = true;
                }

                if (isConstructible) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sAddCraftedItem", "Add Crafted Item"))) {
                        FormActions::AddConstructedItemToPlayer(selectedRecord->formID);
                    }
                    hasActionOnRow = true;
                }

                if (isEquippable) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(context.localize("General", "sEquipItem", "Equip Item"))) {
                        char command[64]{};
                        std::snprintf(command, sizeof(command), "player.equipitem %08X", selectedRecord->formID);
                        FormActions::ExecuteConsoleCommand(command);
                    }
                    hasActionOnRow = true;
                }

                if (canTeleport) {
                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }

                    auto* teleportForm = RE::TESForm::GetFormByID(selectedRecord->formID);
                    const char* editorID = teleportForm ? teleportForm->GetFormEditorID() : nullptr;
                    const bool canUseCoc = editorID && editorID[0] != '\0';

                    if (canUseCoc) {
                        if (ImGui::Button(context.localize("General", "sTeleportCOC", "Teleport (COC)"))) {
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
