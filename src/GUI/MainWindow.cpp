#include "GUI/MainWindow.h"

#include "Config/Config.h"
#include "Data/DataManager.h"
#include "GUI/Tabs/SettingsTab.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/SearchBar.h"
#include "Localization/Language.h"

#include <imgui.h>

#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESNPC.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESSound.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWeightForm.h>

#include <cctype>
#include <cstdio>
#include <algorithm>

namespace ESPExplorerAE
{
    namespace
    {
        struct ItemSortState
        {
            int column{ 1 };
            bool ascending{ true };
        };

        std::string pluginSearch{};
        std::string itemSearch{};
        std::string selectedPluginFilter{};
        ItemSortState itemSort{};

        char pluginSearchBuffer[256]{};
        char itemSearchBuffer[256]{};
        char npcSearchBuffer[256]{};
        char objectSearchBuffer[256]{};
        char spellPerkSearchBuffer[256]{};

        std::string npcSearch{};
        std::string objectSearch{};
        std::string spellPerkSearch{};

        std::unordered_set<std::uint32_t> favoriteForms{};
        bool favoritesInitialized{ false };
        std::string activeMainTab{};

        struct ItemGrantPopupState
        {
            bool openRequested{ false };
            bool visible{ false };
            FormEntry entry{};
            int quantity{ 1 };
            int ammoQuantity{ 100 };
            std::uint32_t ammoFormID{ 0 };
            bool includeAmmo{ false };
        };

        ItemGrantPopupState itemGrantPopup{};
        bool playerGodModeEnabled{ false };
        bool playerNoClipEnabled{ false };
        int playerCurrentWeaponAmmoAmount{ 200 };
        int playerAllAmmoAmount{ 100 };
        std::uint32_t selectedPluginTreeRecordFormID{ 0 };
        bool showPlayableRecords{ true };
        bool showNonPlayableRecords{ true };
        bool showNamedRecords{ true };
        bool showUnnamedRecords{ true };
        bool showDeletedRecords{ true };
        std::unordered_map<std::string, std::uint32_t> selectedItemRows{};
        std::uint64_t pluginBrowserCacheVersion{ 0 };
        std::string pluginBrowserCacheSearch{};
        std::string pluginBrowserCacheSelectedPlugin{};
        bool pluginBrowserCacheShowPlayable{ true };
        bool pluginBrowserCacheShowNonPlayable{ true };
        bool pluginBrowserCacheShowNamed{ true };
        bool pluginBrowserCacheShowUnnamed{ true };
        bool pluginBrowserCacheShowDeleted{ true };
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<const FormEntry*>>> pluginBrowserGroupedRecordsCache{};
        std::vector<std::string> pluginBrowserOrderedPluginsCache{};

        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        }

        void ResetQuickFilters()
        {
            selectedPluginFilter.clear();
            pluginSearch.clear();
            itemSearch.clear();
            npcSearch.clear();
            objectSearch.clear();
            spellPerkSearch.clear();

            pluginSearchBuffer[0] = '\0';
            itemSearchBuffer[0] = '\0';
            npcSearchBuffer[0] = '\0';
            objectSearchBuffer[0] = '\0';
            spellPerkSearchBuffer[0] = '\0';
        }

        void DrawPluginFilterStatus()
        {
            ImGui::Text("%s: %s", L("PluginBrowser", "sFilter", "Filter"), selectedPluginFilter.empty() ? L("General", "sNone", "None") : selectedPluginFilter.c_str());
        }

        float CalcButtonWidth(const char* label, float minimumWidth = 0.0f)
        {
            const auto& style = ImGui::GetStyle();
            const float width = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x;
            return (std::max)(width, minimumWidth);
        }

        bool DrawLocalizedFilterToggles(std::string_view idSuffix)
        {
            bool changed = false;

            const std::string playableLabel = std::string(L("General", "sPlayable", "Playable")) + "##Playable" + std::string(idSuffix);
            if (ImGui::Checkbox(playableLabel.c_str(), &showPlayableRecords)) {
                changed = true;
            }
            ImGui::SameLine();

            const std::string nonPlayableLabel = std::string(L("General", "sNonPlayable", "Non-Playable")) + "##NonPlayable" + std::string(idSuffix);
            if (ImGui::Checkbox(nonPlayableLabel.c_str(), &showNonPlayableRecords)) {
                changed = true;
            }
            ImGui::SameLine();

            const std::string namedLabel = std::string(L("General", "sNamed", "Named")) + "##Named" + std::string(idSuffix);
            if (ImGui::Checkbox(namedLabel.c_str(), &showNamedRecords)) {
                changed = true;
            }
            ImGui::SameLine();

            const std::string unnamedLabel = std::string(L("General", "sUnnamedLabel", "Unnamed")) + "##Unnamed" + std::string(idSuffix);
            if (ImGui::Checkbox(unnamedLabel.c_str(), &showUnnamedRecords)) {
                changed = true;
            }
            ImGui::SameLine();

            const std::string deletedLabel = std::string(L("General", "sDeleted", "Deleted")) + "##Deleted" + std::string(idSuffix);
            if (ImGui::Checkbox(deletedLabel.c_str(), &showDeletedRecords)) {
                changed = true;
            }

            return changed;
        }

        void EnsureFavoritesLoaded()
        {
            if (favoritesInitialized) {
                return;
            }

            const auto& settings = Config::Get();
            favoriteForms.clear();
            favoriteForms.insert(settings.favorites.begin(), settings.favorites.end());
            activeMainTab = settings.lastActiveTab;
            showPlayableRecords = settings.listShowPlayable;
            showNonPlayableRecords = settings.listShowNonPlayable;
            showNamedRecords = settings.listShowNamed;
            showUnnamedRecords = settings.listShowUnnamed;
            showDeletedRecords = settings.listShowDeleted;
            favoritesInitialized = true;
        }

        void PersistFavoriteForms()
        {
            auto& settings = Config::GetMutable();
            settings.favorites.assign(favoriteForms.begin(), favoriteForms.end());
            Config::Save();
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

        const char* PluginTypeColorTag(std::string_view type)
        {
            if (type == "ESM") {
                return "[M]";
            }
            if (type == "ESL") {
                return "[L]";
            }
            return "[P]";
        }

        ImVec4 PluginTypeColor(std::string_view type)
        {
            if (type == "ESM") {
                return ImVec4(0.95f, 0.78f, 0.31f, 1.0f);
            }
            if (type == "ESL") {
                return ImVec4(0.52f, 0.76f, 0.99f, 1.0f);
            }
            return ImVec4(0.64f, 0.92f, 0.64f, 1.0f);
        }

        void GiveItemToPlayer(std::uint32_t formID, int count)
        {
            FormActions::GiveToPlayer(formID, static_cast<std::uint32_t>(count));
        }

        void OpenItemGrantPopup(const FormEntry& entry)
        {
            itemGrantPopup.entry = entry;
            itemGrantPopup.quantity = 1;
            itemGrantPopup.ammoQuantity = 100;
            itemGrantPopup.ammoFormID = FormActions::GetWeaponAmmoFormID(entry.formID);
            itemGrantPopup.includeAmmo = (entry.category == "Weapon" || entry.category == "Weapons") && itemGrantPopup.ammoFormID != 0;
            itemGrantPopup.openRequested = true;
            itemGrantPopup.visible = true;
        }

        void RenderItemGrantPopup()
        {
            if (itemGrantPopup.openRequested) {
                ImGui::OpenPopup(L("Items", "sGivePopupTitle", "Add Item"));
                itemGrantPopup.openRequested = false;
            }

            ImGui::SetNextWindowSize(ImVec2(460.0f, 320.0f), ImGuiCond_Appearing);
            if (!ImGui::BeginPopupModal(L("Items", "sGivePopupTitle", "Add Item"), &itemGrantPopup.visible)) {
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                itemGrantPopup.visible = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            ImGui::TextUnformatted(itemGrantPopup.entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : itemGrantPopup.entry.name.c_str());
            ImGui::TextDisabled("%08X", itemGrantPopup.entry.formID);

            ImGui::Separator();

            ImGui::TextUnformatted(L("Items", "sQuantity", "Quantity"));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::SliderInt("##ItemQtySlider", &itemGrantPopup.quantity, 1, 100, "%d");
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputInt("##ItemQtyInput", &itemGrantPopup.quantity, 1, 10);
            if (itemGrantPopup.quantity < 1) {
                itemGrantPopup.quantity = 1;
            }

            ImGui::SameLine();
            if (ImGui::Button("1##ItemQtyPreset")) {
                itemGrantPopup.quantity = 1;
            }
            ImGui::SameLine();
            if (ImGui::Button("10##ItemQtyPreset")) {
                itemGrantPopup.quantity = 10;
            }
            ImGui::SameLine();
            if (ImGui::Button("100##ItemQtyPreset")) {
                itemGrantPopup.quantity = 100;
            }

            if (itemGrantPopup.includeAmmo) {
                ImGui::Separator();
                ImGui::TextUnformatted(L("Items", "sAmmoQuantity", "Ammo Quantity"));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderInt("##AmmoQtySlider", &itemGrantPopup.ammoQuantity, 0, 1000, "%d");
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputInt("##AmmoQtyInput", &itemGrantPopup.ammoQuantity, 10, 100);
                if (itemGrantPopup.ammoQuantity < 0) {
                    itemGrantPopup.ammoQuantity = 0;
                }

                ImGui::SameLine();
                if (ImGui::Button("0##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("10##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 10;
                }
                ImGui::SameLine();
                if (ImGui::Button("50##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 50;
                }
                ImGui::SameLine();
                if (ImGui::Button("100##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 100;
                }
                ImGui::SameLine();
                if (ImGui::Button("500##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 500;
                }
            }

            ImGui::Separator();
            const char* giveToPlayerLabel = L("Items", "sGiveToPlayer", "Give To Player");
            const char* spawnAtPlayerLabel = L("NPCs", "sSpawnAtPlayer", "Spawn At Player");
            const char* cancelLabel = L("General", "sCancel", "Cancel");
            const float popupButtonMinWidth = 96.0f;

            if (ImGui::Button(giveToPlayerLabel, ImVec2(CalcButtonWidth(giveToPlayerLabel, popupButtonMinWidth), 0.0f))) {
                if (itemGrantPopup.includeAmmo && itemGrantPopup.ammoQuantity > 0) {
                    FormActions::GiveToPlayerWithAmmo(
                        itemGrantPopup.entry.formID,
                        static_cast<std::uint32_t>(itemGrantPopup.quantity),
                        itemGrantPopup.ammoFormID,
                        static_cast<std::uint32_t>(itemGrantPopup.ammoQuantity));
                } else {
                    GiveItemToPlayer(itemGrantPopup.entry.formID, itemGrantPopup.quantity);
                }
                itemGrantPopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(spawnAtPlayerLabel, ImVec2(CalcButtonWidth(spawnAtPlayerLabel, popupButtonMinWidth), 0.0f))) {
                FormActions::SpawnAtPlayer(itemGrantPopup.entry.formID, static_cast<std::uint32_t>(itemGrantPopup.quantity));
                itemGrantPopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(cancelLabel, ImVec2(CalcButtonWidth(cancelLabel, popupButtonMinWidth), 0.0f))) {
                itemGrantPopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        void DrawPlayerTab(const FormCache& cache)
        {
            if (ImGui::Button(L("Player", "sRefillHealth", "Refill Health"))) {
                FormActions::ExecuteConsoleCommand("player.resethealth");
            }

            ImGui::SameLine();
            if (ImGui::Button(playerGodModeEnabled ? L("Player", "sGodModeOff", "Godmode: ON") : L("Player", "sGodModeOn", "Godmode: OFF"))) {
                FormActions::ExecuteConsoleCommand("tgm");
                playerGodModeEnabled = !playerGodModeEnabled;
            }

            ImGui::SameLine();
            if (ImGui::Button(playerNoClipEnabled ? L("Player", "sNoClipOff", "Noclip: ON") : L("Player", "sNoClipOn", "Noclip: OFF"))) {
                FormActions::ExecuteConsoleCommand("tcl");
                playerNoClipEnabled = !playerNoClipEnabled;
            }

            ImGui::Separator();

            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputInt(L("Player", "sCurrentWeaponAmmo", "Current Weapon Ammo"), &playerCurrentWeaponAmmoAmount, 10, 100);
            if (playerCurrentWeaponAmmoAmount < 1) {
                playerCurrentWeaponAmmoAmount = 1;
            }
            ImGui::SameLine();
            if (ImGui::Button(L("Player", "sAddCurrentAmmo", "Add Ammo For Held Weapon"))) {
                FormActions::AddAmmoForCurrentWeapon(static_cast<std::uint32_t>(playerCurrentWeaponAmmoAmount));
            }

            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputInt(L("Player", "sAllAmmoCount", "All Ammo Count"), &playerAllAmmoAmount, 10, 100);
            if (playerAllAmmoAmount < 1) {
                playerAllAmmoAmount = 1;
            }
            ImGui::SameLine();
            if (ImGui::Button(L("Player", "sAddAllAmmo", "Add All Ammo Types"))) {
                for (const auto& ammo : cache.ammo) {
                    FormActions::GiveToPlayer(ammo.formID, static_cast<std::uint32_t>(playerAllAmmoAmount));
                }
            }

            ImGui::Separator();

            if (ImGui::Button(L("Player", "sAddStimpak", "Add Stimpaks"))) {
                FormEntry entry{};
                entry.formID = FormActions::kStimpakFormID;
                entry.name = L("Player", "sItemStimpak", "Stimpak");
                entry.category = "Aid";
                OpenItemGrantPopup(entry);
            }

            ImGui::SameLine();
            if (ImGui::Button(L("Player", "sAddLockpick", "Add Lockpicks"))) {
                FormEntry entry{};
                entry.formID = FormActions::kLockpickFormID;
                entry.name = L("Player", "sItemLockpick", "Lockpick");
                entry.category = "Misc";
                OpenItemGrantPopup(entry);
            }
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

        std::string CategoryDisplayName(std::string_view category)
        {
            if (category == "WEAP" || category == "Weapon") {
                return L("Items", "sWeapons", "Weapons");
            }
            if (category == "ARMO" || category == "Armor") {
                return L("Items", "sArmor", "Armor");
            }
            if (category == "AMMO" || category == "Ammo") {
                return L("General", "sAmmunition", "Ammunition");
            }
            if (category == "ALCH") {
                return L("General", "sAidChems", "Aid/Chems");
            }
            if (category == "BOOK") {
                return L("General", "sBooks", "Books");
            }
            if (category == "MISC" || category == "Misc") {
                return L("General", "sMiscellaneous", "Miscellaneous");
            }
            if (category == "KEYM") {
                return L("General", "sKeys", "Keys");
            }
            if (category == "NOTE") {
                return L("General", "sHolotapesNotes", "Holotapes/Notes");
            }
            if (category == "NPC" || category == "NPC_") {
                return L("NPCs", "sTabName", "NPCs");
            }
            if (category == "LVLN") {
                return L("General", "sLeveledNPCs", "Leveled NPCs");
            }
            if (category == "ACTI" || category == "Activator") {
                return L("Objects", "sActivators", "Activators");
            }
            if (category == "CONT" || category == "Container") {
                return L("Objects", "sContainers", "Containers");
            }
            if (category == "STAT" || category == "Static") {
                return L("General", "sStaticObjects", "Static Objects");
            }
            if (category == "FURN" || category == "Furniture") {
                return L("Objects", "sFurniture", "Furniture");
            }
            if (category == "SPEL" || category == "Spell") {
                return L("Spells", "sSpells", "Spells");
            }
            if (category == "PERK" || category == "Perk") {
                return L("Spells", "sPerks", "Perks");
            }
            if (category == "SNDR") {
                return L("General", "sSoundDescriptors", "Sound Descriptors");
            }
            if (category == "SOUN") {
                return L("General", "sSounds", "Sounds");
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

        bool PassesLocalRecordFilters(const FormEntry& entry)
        {
            if (!showPlayableRecords && entry.isPlayable) {
                return false;
            }
            if (!showNonPlayableRecords && !entry.isPlayable) {
                return false;
            }

            const bool hasName = !entry.name.empty();
            if (!showNamedRecords && hasName) {
                return false;
            }
            if (!showUnnamedRecords && !hasName) {
                return false;
            }

            if (!showDeletedRecords && entry.isDeleted) {
                return false;
            }

            return true;
        }

        std::vector<FormEntry> ApplyLocalRecordFilters(std::vector<FormEntry> entries)
        {
            entries.erase(std::remove_if(entries.begin(), entries.end(), [](const FormEntry& entry) {
                return !PassesLocalRecordFilters(entry);
            }), entries.end());
            return entries;
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

        void DrawPluginBrowser(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion)
        {
            bool listFilterSettingsChanged = false;

            if (ImGui::InputText(L("PluginBrowser", "sSearch", "Plugin Search"), pluginSearchBuffer, sizeof(pluginSearchBuffer))) {
                pluginSearch = pluginSearchBuffer;
            }
            ImGui::SameLine();

            if (ImGui::Button(L("PluginBrowser", "sClearFilter", "Clear Plugin Filter"))) {
                selectedPluginFilter.clear();
            }

            if (!selectedPluginFilter.empty()) {
                ImGui::SameLine();
                ImGui::Text("%s: %s", L("PluginBrowser", "sActiveFilter", "Active"), selectedPluginFilter.c_str());
            }

            if (DrawLocalizedFilterToggles("PluginBrowser")) {
                listFilterSettingsChanged = true;
            }

            if (listFilterSettingsChanged) {
                auto& settings = Config::GetMutable();
                settings.listShowPlayable = showPlayableRecords;
                settings.listShowNonPlayable = showNonPlayableRecords;
                settings.listShowNamed = showNamedRecords;
                settings.listShowUnnamed = showUnnamedRecords;
                settings.listShowDeleted = showDeletedRecords;
                Config::Save();
            }

            const bool needsCacheRebuild =
                pluginBrowserCacheVersion != dataVersion ||
                pluginBrowserCacheSearch != pluginSearch ||
                pluginBrowserCacheSelectedPlugin != selectedPluginFilter ||
                pluginBrowserCacheShowPlayable != showPlayableRecords ||
                pluginBrowserCacheShowNonPlayable != showNonPlayableRecords ||
                pluginBrowserCacheShowNamed != showNamedRecords ||
                pluginBrowserCacheShowUnnamed != showUnnamedRecords ||
                pluginBrowserCacheShowDeleted != showDeletedRecords;

            if (needsCacheRebuild) {
                pluginBrowserGroupedRecordsCache.clear();
                pluginBrowserGroupedRecordsCache.reserve(plugins.size());

                for (const auto& entry : cache.allRecords) {
                    if (!PassesLocalRecordFilters(entry)) {
                        continue;
                    }

                    const std::string pluginName = entry.sourcePlugin.empty() ? std::string(L("General", "sUnknown", "<Unknown>")) : entry.sourcePlugin;

                    if (!selectedPluginFilter.empty() && pluginName != selectedPluginFilter) {
                        continue;
                    }

                    if (!pluginSearch.empty() &&
                        !ContainsCaseInsensitive(pluginName, pluginSearch) &&
                        !ContainsCaseInsensitive(entry.name, pluginSearch) &&
                        !ContainsCaseInsensitive(entry.category, pluginSearch)) {
                        continue;
                    }

                    pluginBrowserGroupedRecordsCache[pluginName][entry.category].push_back(&entry);
                }

                pluginBrowserOrderedPluginsCache.clear();
                pluginBrowserOrderedPluginsCache.reserve(pluginBrowserGroupedRecordsCache.size());
                std::unordered_set<std::string> seenPlugins{};
                seenPlugins.reserve(pluginBrowserGroupedRecordsCache.size());

                for (const auto& plugin : plugins) {
                    if (pluginBrowserGroupedRecordsCache.contains(plugin.filename)) {
                        pluginBrowserOrderedPluginsCache.push_back(plugin.filename);
                        seenPlugins.insert(plugin.filename);
                    }
                }

                for (const auto& [pluginName, _] : pluginBrowserGroupedRecordsCache) {
                    if (!seenPlugins.contains(pluginName)) {
                        pluginBrowserOrderedPluginsCache.push_back(pluginName);
                    }
                }

                pluginBrowserCacheVersion = dataVersion;
                pluginBrowserCacheSearch = pluginSearch;
                pluginBrowserCacheSelectedPlugin = selectedPluginFilter;
                pluginBrowserCacheShowPlayable = showPlayableRecords;
                pluginBrowserCacheShowNonPlayable = showNonPlayableRecords;
                pluginBrowserCacheShowNamed = showNamedRecords;
                pluginBrowserCacheShowUnnamed = showUnnamedRecords;
                pluginBrowserCacheShowDeleted = showDeletedRecords;
            }

            const float leftWidth = ImGui::GetContentRegionAvail().x * 0.58f;

            if (ImGui::BeginChild("PluginTreeLeft", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders)) {
                for (const auto& pluginName : pluginBrowserOrderedPluginsCache) {
                    auto pluginIt = pluginBrowserGroupedRecordsCache.find(pluginName);
                    if (pluginIt == pluginBrowserGroupedRecordsCache.end()) {
                        continue;
                    }

                    std::size_t totalRecords = 0;
                    for (const auto& [_, list] : pluginIt->second) {
                        totalRecords += list.size();
                    }

                    const std::string pluginLabel = pluginName + " (" + std::to_string(totalRecords) + ")";
                    if (ImGui::TreeNode(pluginLabel.c_str())) {
                        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                            selectedPluginFilter = pluginName;
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

                            const auto displayCategory = CategoryDisplayName(category);
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

                                    const auto* displayName = record->name.empty() ? L("General", "sUnnamed", "<Unnamed>") : record->name.c_str();
                                    char recordLabel[512]{};
                                    std::snprintf(recordLabel, sizeof(recordLabel), "%s [%08X]##TreeRecord%08X", displayName, record->formID, record->formID);

                                    const bool isSelected = selectedPluginTreeRecordFormID == record->formID;
                                    if (ImGui::Selectable(recordLabel, isSelected)) {
                                        selectedPluginTreeRecordFormID = record->formID;
                                    }

                                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && CanGiveFromTreeCategory(record->category)) {
                                        OpenItemGrantPopup(*record);
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
                if (selectedPluginTreeRecordFormID != 0) {
                    for (const auto& entry : cache.allRecords) {
                        if (entry.formID == selectedPluginTreeRecordFormID) {
                            selectedRecord = &entry;
                            break;
                        }
                    }
                }

                if (!selectedRecord) {
                    ImGui::TextUnformatted(L("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
                } else {
                    ImGui::TextUnformatted(selectedRecord->name.empty() ? L("General", "sUnnamed", "<Unnamed>") : selectedRecord->name.c_str());
                    ImGui::TextDisabled("%08X", selectedRecord->formID);
                    ImGui::Separator();

                    ImGui::Text("%s: %s", L("General", "sPlugin", "Plugin"), selectedRecord->sourcePlugin.c_str());
                    const auto displayCategory = CategoryDisplayName(selectedRecord->category);
                    if (displayCategory == selectedRecord->category) {
                        ImGui::Text("%s: %s", L("General", "sCategory", "Category"), selectedRecord->category.c_str());
                    } else {
                        ImGui::Text("%s: %s [%s]", L("General", "sCategory", "Category"), displayCategory.data(), selectedRecord->category.c_str());
                    }
                    ImGui::Text("%s: %s", L("General", "sPlayable", "Playable"), selectedRecord->isPlayable ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));
                    ImGui::Text("%s: %s", L("General", "sDeleted", "Deleted"), selectedRecord->isDeleted ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));

                    auto* form = RE::TESForm::GetFormByID(selectedRecord->formID);
                    if (form) {
                        const auto* editorID = form->GetFormEditorID();
                        ImGui::Text("%s: %s", L("General", "sEditorID", "EditorID"), (editorID && editorID[0] != '\0') ? editorID : L("General", "sNone", "None"));
                        ImGui::Text("%s: %s", L("General", "sType", "Type"), form->GetFormTypeString());
                        ImGui::Text("%s: %u", L("General", "sReferenceCount", "Reference Count"), form->GetRefCount());

                        if (const auto* valueForm = form->As<RE::TESValueForm>()) {
                            ImGui::Text("%s: %d", L("General", "sValue", "Value"), valueForm->GetFormValue());
                        }

                        if (const auto* weightForm = form->As<RE::TESWeightForm>()) {
                            ImGui::Text("%s: %.2f", L("General", "sWeight", "Weight"), weightForm->GetFormWeight());
                        }

                        if (const auto* weaponForm = form->As<RE::TESObjectWEAP>()) {
                            ImGui::Text("%s: %u", L("General", "sDamage", "Damage"), weaponForm->weaponData.attackDamage);
                            if (weaponForm->weaponData.attackSeconds > 0.0f) {
                                ImGui::Text("%s: %.2f", L("General", "sFireRate", "Fire Rate"), 1.0f / weaponForm->weaponData.attackSeconds);
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
                                    ImGui::Text("%s: %s", L("Items", "sAmmo", "Ammo"), ammoName.c_str());
                                } else {
                                    ImGui::Text("%s: %08X", L("Items", "sAmmo", "Ammo"), ammoFormID);
                                }
                            }
                        }

                        if (const auto* armorForm = form->As<RE::TESObjectARMO>()) {
                            ImGui::Text("%s: %u", L("General", "sArmorRating", "Armor Rating"), armorForm->armorData.rating);
                        }

                        if (const auto* npcForm = form->As<RE::TESNPC>()) {
                            ImGui::Text("%s: %d", L("General", "sLevel", "Level"), npcForm->GetLevel());
                            if (const auto* raceForm = npcForm->GetFormRace()) {
                                const auto raceName = RE::TESFullName::GetFullName(*raceForm);
                                if (!raceName.empty()) {
                                    const std::string raceNameString{ raceName };
                                    ImGui::Text("%s: %s", L("General", "sRace", "Race"), raceNameString.c_str());
                                }
                            }
                        }

                        if (const auto* soundForm = form->As<RE::TESSound>()) {
                            if (soundForm->descriptor) {
                                const auto* descriptorEditorID = soundForm->descriptor->GetFormEditorID();
                                if (descriptorEditorID && descriptorEditorID[0] != '\0') {
                                    ImGui::Text("%s: %s", L("General", "sDescriptor", "Descriptor"), descriptorEditorID);
                                } else {
                                    ImGui::Text("%s: %08X", L("General", "sDescriptor", "Descriptor"), soundForm->descriptor->GetFormID());
                                }
                            } else {
                                ImGui::TextDisabled("%s: %s", L("General", "sDescriptor", "Descriptor"), L("General", "sNone", "None"));
                            }
                        }

                        if (const auto keywordForm = form->As<RE::BGSKeywordForm>()) {
                            ImGui::Separator();
                            ImGui::TextUnformatted(L("General", "sKeywords", "Keywords"));
                            int displayed = 0;
                            keywordForm->ForEachKeyword([&displayed](RE::BGSKeyword* keyword) {
                                if (!keyword || displayed >= 40) {
                                    return displayed >= 40 ? RE::BSContainer::ForEachResult::kStop : RE::BSContainer::ForEachResult::kContinue;
                                }
                                const auto text = keyword->formEditorID.c_str();
                                ImGui::BulletText("%s", (text && text[0] != '\0') ? text : L("General", "sUnnamedKeyword", "<UnnamedKeyword>"));
                                ++displayed;
                                return RE::BSContainer::ForEachResult::kContinue;
                            });
                            if (displayed == 0) {
                                ImGui::TextDisabled("%s", L("General", "sNoKeywords", "<No keywords>"));
                            }
                        }
                    }

                    ImGui::Separator();
                    const bool canSpawn = CanSpawnFromTreeCategory(selectedRecord->category);
                    const bool canTeleport = CanTeleportFromTreeCategory(selectedRecord->category);
                    const bool isQuest = IsQuestCategory(selectedRecord->category);
                    const bool isPerk = IsPerkCategory(selectedRecord->category);

                    bool hasActionOnRow = false;
                    if (CanGiveFromTreeCategory(selectedRecord->category)) {
                        if (ImGui::Button(L("Items", "sGiveItem", "Give Item"))) {
                            OpenItemGrantPopup(*selectedRecord);
                        }
                        hasActionOnRow = true;
                    }

                    if (canSpawn) {
                        if (hasActionOnRow) {
                            ImGui::SameLine();
                        }
                        if (ImGui::Button(L("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                            FormActions::SpawnAtPlayer(selectedRecord->formID, 1);
                        }
                        hasActionOnRow = true;
                    }

                    if (isQuest) {
                        if (hasActionOnRow) {
                            ImGui::SameLine();
                        }
                        if (ImGui::Button(L("General", "sStartQuest", "Start Quest"))) {
                            char command[64]{};
                            std::snprintf(command, sizeof(command), "startquest %08X", selectedRecord->formID);
                            FormActions::ExecuteConsoleCommand(command);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(L("General", "sCompleteQuest", "Complete Quest"))) {
                            char command[64]{};
                            std::snprintf(command, sizeof(command), "completequest %08X", selectedRecord->formID);
                            FormActions::ExecuteConsoleCommand(command);
                        }
                        hasActionOnRow = true;
                    }

                    if (isPerk) {
                        if (hasActionOnRow) {
                            ImGui::SameLine();
                        }
                        if (ImGui::Button(L("General", "sAddPerk", "Add Perk"))) {
                            FormActions::AddPerkToPlayer(selectedRecord->formID);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(L("General", "sRemovePerk", "Remove Perk"))) {
                            FormActions::RemovePerkFromPlayer(selectedRecord->formID);
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
                            if (ImGui::Button(L("General", "sTeleportCOC", "Teleport (COC)"))) {
                                std::string command = std::string("coc ") + editorID;
                                FormActions::ExecuteConsoleCommand(command);
                            }
                        } else {
                            ImGui::BeginDisabled(true);
                            ImGui::Button(L("General", "sTeleportCOC", "Teleport (COC)"));
                            ImGui::EndDisabled();
                        }
                        hasActionOnRow = true;
                    }

                    if (hasActionOnRow) {
                        ImGui::SameLine();
                    }
                    if (ImGui::Button(L("General", "sCopyFormID", "Copy FormID"))) {
                        FormActions::CopyFormID(selectedRecord->formID);
                    }
                }
            }
            ImGui::EndChild();
        }

        void DrawItemTable(std::string_view tableId, std::vector<FormEntry> items)
        {
            auto& selectedItemFormID = selectedItemRows[std::string(tableId)];

            items.erase(std::remove_if(items.begin(), items.end(), [](const FormEntry& entry) {
                return !PassesLocalRecordFilters(entry);
            }), items.end());

            if (!selectedPluginFilter.empty()) {
                items.erase(std::remove_if(items.begin(), items.end(), [](const FormEntry& entry) {
                    return entry.sourcePlugin != selectedPluginFilter;
                }), items.end());
            }

            if (!itemSearch.empty()) {
                items.erase(std::remove_if(items.begin(), items.end(), [](const FormEntry& entry) {
                    return !ContainsCaseInsensitive(entry.name, itemSearch) && !ContainsCaseInsensitive(entry.sourcePlugin, itemSearch);
                }), items.end());
            }

            if (!items.empty()) {
                std::ranges::sort(items, [](const FormEntry& left, const FormEntry& right) {
                    const auto compareBy = [&](int column) {
                        switch (column) {
                        case 0:
                            if (left.formID < right.formID) {
                                return -1;
                            }
                            if (left.formID > right.formID) {
                                return 1;
                            }
                            return 0;
                        case 1:
                            return left.name.compare(right.name);
                        case 2:
                            return left.category.compare(right.category);
                        case 3:
                            return left.sourcePlugin.compare(right.sourcePlugin);
                        case 4:
                            return static_cast<int>(left.isPlayable) - static_cast<int>(right.isPlayable);
                        case 5:
                            return static_cast<int>(left.isDeleted) - static_cast<int>(right.isDeleted);
                        default:
                            return left.name.compare(right.name);
                        }
                    };

                    const int cmp = compareBy(itemSort.column);
                    if (cmp == 0) {
                        return left.formID < right.formID;
                    }
                    return itemSort.ascending ? (cmp < 0) : (cmp > 0);
                });
            }

            const float actionBarReserve = ImGui::GetFrameHeightWithSpacing() + 10.0f;
            const auto availableHeight = ImGui::GetContentRegionAvail().y - actionBarReserve;
            const auto tableHeight = availableHeight > 180.0f ? availableHeight : 180.0f;
            if (ImGui::BeginTable(tableId.data(), 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
                ImGui::TableSetupColumn(L("General", "sFormID", "FormID"));
                ImGui::TableSetupColumn(L("General", "sName", "Name"), ImGuiTableColumnFlags_DefaultSort);
                ImGui::TableSetupColumn(L("General", "sCategory", "Category"));
                ImGui::TableSetupColumn(L("General", "sPlugin", "Plugin"));
                ImGui::TableSetupColumn(L("General", "sPlayable", "Playable"));
                ImGui::TableSetupColumn(L("General", "sDeleted", "Deleted"));
                ImGui::TableHeadersRow();

                if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0 && sortSpecs->SpecsDirty) {
                    itemSort.column = sortSpecs->Specs[0].ColumnIndex;
                    itemSort.ascending = (sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                    sortSpecs->SpecsDirty = false;
                }

                char formIDBuffer[16]{};

                for (const auto& entry : items) {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);
                    const bool isRowSelected = selectedItemFormID == entry.formID;
                    if (ImGui::Selectable(formIDBuffer, isRowSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        selectedItemFormID = entry.formID;
                        ImGui::SetClipboardText(formIDBuffer);
                    }

                    ImGui::TableSetColumnIndex(1);
                    const auto* entryName = entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : entry.name.c_str();
                    ImGui::TextUnformatted(entryName);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        selectedItemFormID = entry.formID;
                        OpenItemGrantPopup(entry);
                    }

                    const std::string rowPopupId = "ItemRowContext##" + std::to_string(entry.formID) + std::string(tableId);
                    if (ImGui::BeginPopupContextItem(rowPopupId.c_str())) {
                        if (ImGui::MenuItem(L("Items", "sGiveItem", "Give Item"))) {
                            OpenItemGrantPopup(entry);
                        }

                        if (ImGui::MenuItem(L("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                            FormActions::SpawnAtPlayer(entry.formID, 1);
                        }

                        ImGui::Separator();
                        if (ImGui::MenuItem(L("General", "sCopyFormID", "Copy FormID"))) {
                            FormActions::CopyFormID(entry.formID);
                        }

                        if (const char* editorID = TryGetEditorID(entry.formID)) {
                            if (ImGui::MenuItem(L("General", "sCopyEditorID", "Copy EditorID"))) {
                                ImGui::SetClipboardText(editorID);
                            }
                        }

                        if (ImGui::MenuItem(L("General", "sCopyRecordSource", "Copy Record Source"))) {
                            ImGui::SetClipboardText(entry.sourcePlugin.c_str());
                        }

                        ImGui::EndPopup();
                    }

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(entry.category.c_str());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(entry.sourcePlugin.c_str());

                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(entry.isPlayable ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));

                    ImGui::TableSetColumnIndex(5);
                    ImGui::TextUnformatted(entry.isDeleted ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));
                }

                ImGui::EndTable();
            }

            const auto selectedIt = std::find_if(items.begin(), items.end(), [&](const FormEntry& entry) {
                return entry.formID == selectedItemFormID;
            });

            if (selectedIt != items.end()) {
                if (ImGui::Button(L("Items", "sGiveItem", "Give Item"))) {
                    OpenItemGrantPopup(*selectedIt);
                }
                ImGui::SameLine();
                if (ImGui::Button(L("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                    FormActions::SpawnAtPlayer(selectedIt->formID, 1);
                }
                ImGui::SameLine();
                if (ImGui::Button(L("General", "sCopyFormID", "Copy FormID"))) {
                    FormActions::CopyFormID(selectedIt->formID);
                }
                if (const char* editorID = TryGetEditorID(selectedIt->formID)) {
                    ImGui::SameLine();
                    if (ImGui::Button(L("General", "sCopyEditorID", "Copy EditorID"))) {
                        ImGui::SetClipboardText(editorID);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(L("General", "sCopyRecordSource", "Copy Record Source"))) {
                    ImGui::SetClipboardText(selectedIt->sourcePlugin.c_str());
                }
            }
        }

        void DrawItemBrowser(const FormCache& cache)
        {
            auto& settings = Config::GetMutable();
            bool listFilterSettingsChanged = false;

            if (DrawLocalizedFilterToggles("ItemBrowser")) {
                listFilterSettingsChanged = true;
            }

            if (listFilterSettingsChanged) {
                settings.listShowPlayable = showPlayableRecords;
                settings.listShowNonPlayable = showNonPlayableRecords;
                settings.listShowNamed = showNamedRecords;
                settings.listShowUnnamed = showUnnamedRecords;
                settings.listShowDeleted = showDeletedRecords;
                Config::Save();
            }

            if (ImGui::InputText(L("Items", "sSearch", "Item Search"), itemSearchBuffer, sizeof(itemSearchBuffer))) {
                itemSearch = itemSearchBuffer;
            }

            DrawPluginFilterStatus();

            if (ImGui::BeginTabBar("ItemCategories")) {
                if (ImGui::BeginTabItem(L("Items", "sWeapons", "Weapons"))) {
                    DrawItemTable("ItemTableWeapons", cache.weapons);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(L("Items", "sArmor", "Armor"))) {
                    DrawItemTable("ItemTableArmor", cache.armors);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(L("Items", "sAmmo", "Ammo"))) {
                    DrawItemTable("ItemTableAmmo", cache.ammo);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(L("Items", "sMisc", "Misc"))) {
                    DrawItemTable("ItemTableMisc", cache.misc);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        void DrawNPCBrowser(const FormCache& cache)
        {
            SearchBar::Draw(L("NPCs", "sSearch", "NPC Search"), npcSearchBuffer, sizeof(npcSearchBuffer), npcSearch);

            const FormTableConfig tableConfig{
                .tableId = "NPCTable",
                .primaryActionLabel = L("NPCs", "sSpawnNPC", "Spawn"),
                .quantityActionLabel = L("NPCs", "sSpawnAtPlayer", "Spawn At Player"),
                .allowFavorites = true
            };

            FormTable::Draw(
                ApplyLocalRecordFilters(cache.npcs),
                npcSearch,
                selectedPluginFilter,
                tableConfig,
                [](const FormEntry& entry) {
                    FormActions::SpawnAtPlayer(entry.formID, 1);
                },
                [](const FormEntry& entry, int quantity) {
                    FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                },
                &favoriteForms);
        }

        void DrawObjectBrowser(const FormCache& cache)
        {
            SearchBar::Draw(L("Objects", "sSearch", "Object Search"), objectSearchBuffer, sizeof(objectSearchBuffer), objectSearch);

            if (ImGui::BeginTabBar("ObjectCategories")) {
                const FormTableConfig activatorConfig{
                    .tableId = "ObjectTableActivators",
                    .primaryActionLabel = L("Objects", "sPlace", "Place"),
                    .quantityActionLabel = L("Objects", "sPlaceAtPlayer", "Place At Player"),
                    .allowFavorites = true
                };
                const FormTableConfig containerConfig{
                    .tableId = "ObjectTableContainers",
                    .primaryActionLabel = L("Objects", "sPlace", "Place"),
                    .quantityActionLabel = L("Objects", "sPlaceAtPlayer", "Place At Player"),
                    .allowFavorites = true
                };
                const FormTableConfig staticConfig{
                    .tableId = "ObjectTableStatics",
                    .primaryActionLabel = L("Objects", "sPlace", "Place"),
                    .quantityActionLabel = L("Objects", "sPlaceAtPlayer", "Place At Player"),
                    .allowFavorites = true
                };
                const FormTableConfig furnitureConfig{
                    .tableId = "ObjectTableFurniture",
                    .primaryActionLabel = L("Objects", "sPlace", "Place"),
                    .quantityActionLabel = L("Objects", "sPlaceAtPlayer", "Place At Player"),
                    .allowFavorites = true
                };

                if (ImGui::BeginTabItem(L("Objects", "sActivators", "Activators"))) {
                    FormTable::Draw(
                        ApplyLocalRecordFilters(cache.activators),
                        objectSearch,
                        selectedPluginFilter,
                        activatorConfig,
                        [](const FormEntry& entry) {
                            FormActions::PlaceAtPlayer(entry.formID, 1);
                        },
                        [](const FormEntry& entry, int quantity) {
                            FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                        },
                        &favoriteForms);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Objects", "sContainers", "Containers"))) {
                    FormTable::Draw(
                        ApplyLocalRecordFilters(cache.containers),
                        objectSearch,
                        selectedPluginFilter,
                        containerConfig,
                        [](const FormEntry& entry) {
                            FormActions::PlaceAtPlayer(entry.formID, 1);
                        },
                        [](const FormEntry& entry, int quantity) {
                            FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                        },
                        &favoriteForms);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Objects", "sStatics", "Statics"))) {
                    FormTable::Draw(
                        ApplyLocalRecordFilters(cache.statics),
                        objectSearch,
                        selectedPluginFilter,
                        staticConfig,
                        [](const FormEntry& entry) {
                            FormActions::PlaceAtPlayer(entry.formID, 1);
                        },
                        [](const FormEntry& entry, int quantity) {
                            FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                        },
                        &favoriteForms);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Objects", "sFurniture", "Furniture"))) {
                    FormTable::Draw(
                        ApplyLocalRecordFilters(cache.furniture),
                        objectSearch,
                        selectedPluginFilter,
                        furnitureConfig,
                        [](const FormEntry& entry) {
                            FormActions::PlaceAtPlayer(entry.formID, 1);
                        },
                        [](const FormEntry& entry, int quantity) {
                            FormActions::PlaceAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                        },
                        &favoriteForms);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        void DrawSpellPerkBrowser(const FormCache& cache)
        {
            SearchBar::Draw(L("Spells", "sSearch", "Spell/Perk Search"), spellPerkSearchBuffer, sizeof(spellPerkSearchBuffer), spellPerkSearch);

            if (ImGui::BeginTabBar("SpellPerkCategories")) {
                const FormTableConfig spellConfig{
                    .tableId = "SpellTable",
                    .primaryActionLabel = L("Spells", "sAddSpell", "Add Spell"),
                    .allowFavorites = true
                };
                const FormTableConfig perkConfig{
                    .tableId = "PerkTable",
                    .primaryActionLabel = L("Spells", "sAddPerk", "Add Perk"),
                    .allowFavorites = true
                };

                if (ImGui::BeginTabItem(L("Spells", "sSpells", "Spells"))) {
                    FormTable::Draw(
                        ApplyLocalRecordFilters(cache.spells),
                        spellPerkSearch,
                        selectedPluginFilter,
                        spellConfig,
                        [](const FormEntry& entry) {
                            FormActions::AddSpellToPlayer(entry.formID);
                        },
                        {},
                        &favoriteForms);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(L("Spells", "sPerks", "Perks"))) {
                    FormTable::Draw(
                        ApplyLocalRecordFilters(cache.perks),
                        spellPerkSearch,
                        selectedPluginFilter,
                        perkConfig,
                        [](const FormEntry& entry) {
                            FormActions::AddPerkToPlayer(entry.formID);
                        },
                        {},
                        &favoriteForms);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
    }

    void MainWindow::Draw()
    {
        EnsureFavoritesLoaded();
        const auto favoritesBefore = favoriteForms;

        const auto& settings = Config::Get();

        ImGui::SetNextWindowPos(ImVec2(settings.windowX, settings.windowY), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(settings.windowW, settings.windowH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(settings.windowAlpha);

        const auto title = Language::Get("General", "sWindowTitle");
        const auto* windowTitle = title.empty() ? "ESP Explorer AE" : title.data();

        if (ImGui::Begin(windowTitle)) {
            bool settingsDirty{ false };

            if (settings.rememberWindowPos) {
                const ImVec2 pos = ImGui::GetWindowPos();
                const ImVec2 size = ImGui::GetWindowSize();

                auto& mutableSettings = Config::GetMutable();
                if (mutableSettings.windowX != pos.x || mutableSettings.windowY != pos.y ||
                    mutableSettings.windowW != size.x || mutableSettings.windowH != size.y) {
                    settingsDirty = true;
                }
                mutableSettings.windowX = pos.x;
                mutableSettings.windowY = pos.y;
                mutableSettings.windowW = size.x;
                mutableSettings.windowH = size.y;
            }

            const auto dataView = DataManager::GetDataView();
            const auto& plugins = dataView.GetPlugins();
            const auto& counts = dataView.GetCounts();
            const auto& cache = dataView.GetFormCache();
            const auto dataVersion = dataView.GetDataVersion();

            const auto totalForms = counts.weapons + counts.armors + counts.ammo + counts.misc + counts.npcs +
                                    counts.activators + counts.containers + counts.statics + counts.furniture + counts.spells + counts.perks;

            char itemTabLabel[64]{};
            char npcTabLabel[64]{};
            char objectTabLabel[64]{};
            char spellPerkTabLabel[64]{};
            std::snprintf(itemTabLabel, sizeof(itemTabLabel), "%s (%zu)###MainTabItem", L("Items", "sBrowserTab", "Item Browser"), counts.weapons + counts.armors + counts.ammo + counts.misc);
            std::snprintf(npcTabLabel, sizeof(npcTabLabel), "%s (%zu)###MainTabNPC", L("NPCs", "sBrowserTab", "NPC Browser"), counts.npcs);
            std::snprintf(objectTabLabel, sizeof(objectTabLabel), "%s (%zu)###MainTabObject", L("Objects", "sBrowserTab", "Object Browser"), counts.activators + counts.containers + counts.statics + counts.furniture);
            std::snprintf(spellPerkTabLabel, sizeof(spellPerkTabLabel), "%s (%zu)###MainTabSpells", L("Spells", "sBrowserTab", "Spells & Perks"), counts.spells + counts.perks);

            const auto& style = ImGui::GetStyle();
            const float footerHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y + style.WindowPadding.y + 8.0f;
            if (ImGui::BeginChild("MainContentRegion", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::BeginTabBar("MainTabs")) {
                    if (ImGui::BeginTabItem(L("PluginBrowser", "sBrowserTab", "Plugin Browser"))) {
                        activeMainTab = "Plugin Browser";
                        if (ImGui::Button(L("General", "sRefreshData", "Refresh Data"))) {
                            DataManager::Refresh();
                        }
                        ImGui::SameLine();
                        ImGui::Text("%s: %zu", L("PluginBrowser", "sPluginsCount", "Plugins"), plugins.size());
                        DrawPluginFilterStatus();

                        DrawPluginBrowser(plugins, cache, dataVersion);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(L("Player", "sTabName", "Player"))) {
                        activeMainTab = "Player";
                        DrawPluginFilterStatus();
                        DrawPlayerTab(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(itemTabLabel)) {
                        activeMainTab = "Item Browser";
                        ImGui::Text("%s: %zu  %s: %zu  %s: %zu  %s: %zu",
                            L("Items", "sWeapons", "Weapons"),
                            counts.weapons,
                            L("Items", "sArmor", "Armor"),
                            counts.armors,
                            L("Items", "sAmmo", "Ammo"),
                            counts.ammo,
                            L("Items", "sMisc", "Misc"),
                            counts.misc);
                        DrawItemBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(npcTabLabel)) {
                        activeMainTab = "NPC Browser";
                        ImGui::Text("%s: %zu", L("NPCs", "sTabName", "NPCs"), counts.npcs);
                        DrawPluginFilterStatus();
                        DrawNPCBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(objectTabLabel)) {
                        activeMainTab = "Object Browser";
                        ImGui::Text("%s: %zu  %s: %zu  %s: %zu  %s: %zu",
                            L("Objects", "sActivators", "Activators"),
                            counts.activators,
                            L("Objects", "sContainers", "Containers"),
                            counts.containers,
                            L("Objects", "sStatics", "Statics"),
                            counts.statics,
                            L("Objects", "sFurniture", "Furniture"),
                            counts.furniture);
                        DrawPluginFilterStatus();
                        DrawObjectBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(spellPerkTabLabel)) {
                        activeMainTab = "Spells & Perks";
                        ImGui::Text("%s: %zu  %s: %zu", L("Spells", "sSpells", "Spells"), counts.spells, L("Spells", "sPerks", "Perks"), counts.perks);
                        DrawPluginFilterStatus();
                        DrawSpellPerkBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const auto settingsTabName = Language::Get("Settings", "sTabName");
                    const auto* settingsLabel = settingsTabName.empty() ? "Settings" : settingsTabName.data();
                    if (ImGui::BeginTabItem(settingsLabel)) {
                        activeMainTab = "Settings";
                        SettingsTab::Draw();
                        ImGui::EndTabItem();
                    }

                    RenderItemGrantPopup();

                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();

            if (ImGui::BeginChild("StatusBarRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGuiIO& io = ImGui::GetIO();
                const float fps = io.Framerate;
                const float frameTime = fps > 0.0f ? (1000.0f / fps) : 0.0f;
                ImGui::Text("%s | %s: %zu  %s: %zu  %s: %zu",
                    L("General", "sStatus", "Status"),
                    L("PluginBrowser", "sPluginsCount", "Plugins"),
                    plugins.size(),
                    L("General", "sForms", "Forms"),
                    totalForms,
                    L("General", "sFavorites", "Favorites"),
                    favoriteForms.size());

                ImGui::SameLine();
                if (ImGui::Button(L("General", "sResetFilters", "Reset Filters"))) {
                    ResetQuickFilters();
                }

                ImGui::SameLine();
                if (ImGui::Button(L("General", "sUndoLastAction", "Undo Last Action")) && FormActions::CanUndoLastAction()) {
                    FormActions::UndoLastAction();
                }

                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && FormActions::CanUndoLastAction()) {
                    FormActions::UndoLastAction();
                }

                if (settings.showFPSInStatus) {
                    ImGui::Text("%s: %.1f  %s: %.2f ms", L("General", "sFPS", "FPS"), fps, L("General", "sFrame", "Frame"), frameTime);
                }
            }
            ImGui::EndChild();

            if (favoriteForms != favoritesBefore) {
                PersistFavoriteForms();
            }

            auto& mutableSettings = Config::GetMutable();
            if (mutableSettings.lastActiveTab != activeMainTab && !activeMainTab.empty()) {
                mutableSettings.lastActiveTab = activeMainTab;
                settingsDirty = true;
            }

            if (settingsDirty) {
                Config::Save();
            }
        }
        ImGui::End();
    }
}
