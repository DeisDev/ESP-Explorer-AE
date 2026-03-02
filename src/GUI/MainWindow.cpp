#include "GUI/MainWindow.h"

#include "Config/Config.h"
#include "Data/DataManager.h"
#include "GUI/Tabs/ItemBrowserTab.h"
#include "GUI/Tabs/LogViewerTab.h"
#include "GUI/Tabs/NPCBrowserTab.h"
#include "GUI/Tabs/ObjectBrowserTab.h"
#include "GUI/Tabs/PlayerTab.h"
#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/SettingsTab.h"
#include "GUI/Tabs/SpellPerkBrowserTab.h"
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
#include <deque>
#include <functional>

namespace ESPExplorerAE
{
    namespace
    {
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
        bool itemSearchCaseSensitive{ false };
        bool npcSearchCaseSensitive{ false };
        bool objectSearchCaseSensitive{ false };
        bool spellPerkSearchCaseSensitive{ false };

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
        bool showUnknownCategories{ false };
        bool pluginGlobalSearchMode{ false };
        bool pluginSearchCaseSensitive{ false };
        std::unordered_map<std::string, std::uint32_t> selectedItemRows{};
        std::uint64_t pluginBrowserCacheVersion{ 0 };
        std::string pluginBrowserCacheSearch{};
        std::string pluginBrowserCacheSelectedPlugin{};
        bool pluginBrowserCacheShowPlayable{ true };
        bool pluginBrowserCacheShowNonPlayable{ true };
        bool pluginBrowserCacheShowNamed{ true };
        bool pluginBrowserCacheShowUnnamed{ true };
        bool pluginBrowserCacheShowDeleted{ true };
        bool pluginBrowserCacheShowUnknown{ false };
        bool pluginBrowserCacheGlobalSearchMode{ false };
        bool pluginBrowserCacheSearchCaseSensitive{ false };
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<const FormEntry*>>> pluginBrowserGroupedRecordsCache{};
        std::vector<std::string> pluginBrowserOrderedPluginsCache{};
        std::vector<const FormEntry*> pluginBrowserGlobalSearchResultsCache{};
        std::deque<std::uint32_t> recentPluginRecordFormIDs{};
        bool refreshDataRequested{ false };
        bool refreshDataInProgress{ false };

        struct ConfirmActionState
        {
            bool openRequested{ false };
            bool visible{ false };
            std::string title{};
            std::string message{};
            std::function<void()> callback{};
        };

        struct GlobalValuePopupState
        {
            bool openRequested{ false };
            bool visible{ false };
            std::uint32_t formID{ 0 };
            std::string editorID{};
            float value{ 0.0f };
        };

        ConfirmActionState confirmAction{};
        GlobalValuePopupState globalValuePopup{};

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

        std::vector<FormEntry> ApplyLocalRecordFilters(std::vector<FormEntry> entries);

        void PersistListFilterSettings()
        {
            auto& settings = Config::GetMutable();
            settings.listShowPlayable = showPlayableRecords;
            settings.listShowNonPlayable = showNonPlayableRecords;
            settings.listShowNamed = showNamedRecords;
            settings.listShowUnnamed = showUnnamedRecords;
            settings.listShowDeleted = showDeletedRecords;
            Config::Save();
        }

        std::vector<FormEntry> ApplyLocalRecordFiltersForTabs(std::vector<FormEntry> entries)
        {
            return ApplyLocalRecordFilters(std::move(entries));
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

        const char* TryGetEditorID(std::uint32_t formID);

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

        const FormEntry* FindRecordByFormID(const FormCache& cache, std::uint32_t formID)
        {
            for (const auto& entry : cache.allRecords) {
                if (entry.formID == formID) {
                    return &entry;
                }
            }

            return nullptr;
        }

        void TrackRecentRecord(std::uint32_t formID)
        {
            if (formID == 0) {
                return;
            }

            recentPluginRecordFormIDs.erase(
                std::remove(recentPluginRecordFormIDs.begin(), recentPluginRecordFormIDs.end(), formID),
                recentPluginRecordFormIDs.end());
            recentPluginRecordFormIDs.push_front(formID);

            constexpr std::size_t kMaxRecentRecords = 50;
            while (recentPluginRecordFormIDs.size() > kMaxRecentRecords) {
                recentPluginRecordFormIDs.pop_back();
            }
        }

        void RequestActionConfirmation(std::string title, std::string message, std::function<void()> callback)
        {
            confirmAction.title = std::move(title);
            confirmAction.message = std::move(message);
            confirmAction.callback = std::move(callback);
            confirmAction.openRequested = true;
            confirmAction.visible = true;
        }

        void OpenGlobalValuePopup(std::uint32_t formID)
        {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (!form) {
                return;
            }

            const auto* editorID = form->GetFormEditorID();
            if (!editorID || editorID[0] == '\0') {
                return;
            }

            globalValuePopup.formID = formID;
            globalValuePopup.editorID = editorID;
            globalValuePopup.value = 0.0f;
            globalValuePopup.openRequested = true;
            globalValuePopup.visible = true;
        }

        void RenderConfirmActionPopup()
        {
            if (confirmAction.openRequested) {
                ImGui::OpenPopup("Confirm Action");
                confirmAction.openRequested = false;
            }

            ImGui::SetNextWindowSize(ImVec2(460.0f, 180.0f), ImGuiCond_Appearing);
            if (!ImGui::BeginPopupModal("Confirm Action", &confirmAction.visible)) {
                return;
            }

            ImGui::TextUnformatted(confirmAction.title.empty() ? "Confirm Action" : confirmAction.title.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", confirmAction.message.c_str());
            ImGui::Spacing();

            if (ImGui::Button(L("General", "sConfirm", "Confirm"), ImVec2(110.0f, 0.0f))) {
                if (confirmAction.callback) {
                    confirmAction.callback();
                }
                confirmAction.callback = {};
                confirmAction.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(L("General", "sCancel", "Cancel"), ImVec2(110.0f, 0.0f))) {
                confirmAction.callback = {};
                confirmAction.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        void RenderGlobalValuePopup()
        {
            if (globalValuePopup.openRequested) {
                ImGui::OpenPopup("Set Global Value");
                globalValuePopup.openRequested = false;
            }

            ImGui::SetNextWindowSize(ImVec2(430.0f, 180.0f), ImGuiCond_Appearing);
            if (!ImGui::BeginPopupModal("Set Global Value", &globalValuePopup.visible)) {
                return;
            }

            ImGui::Text("%s: %s", L("General", "sEditorID", "EditorID"), globalValuePopup.editorID.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputFloat(L("General", "sValue", "Value"), &globalValuePopup.value, 1.0f, 10.0f, "%.3f");
            ImGui::Spacing();

            if (ImGui::Button(L("General", "sApply", "Apply"), ImVec2(100.0f, 0.0f))) {
                char command[256]{};
                std::snprintf(command, sizeof(command), "set %s to %.3f", globalValuePopup.editorID.c_str(), globalValuePopup.value);
                FormActions::ExecuteConsoleCommand(command);
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(L("General", "sCancel", "Cancel"), ImVec2(100.0f, 0.0f))) {
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
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
            PlayerTab::Draw(
                cache,
                playerGodModeEnabled,
                playerNoClipEnabled,
                playerCurrentWeaponAmmoAmount,
                playerAllAmmoAmount,
                [](const FormEntry& entry) {
                    OpenItemGrantPopup(entry);
                },
                L);
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
            PluginBrowserTabContext context{
                .pluginSearch = pluginSearch,
                .pluginSearchBuffer = pluginSearchBuffer,
                .pluginSearchBufferSize = sizeof(pluginSearchBuffer),
                .selectedPluginFilter = selectedPluginFilter,
                .showPlayableRecords = showPlayableRecords,
                .showNonPlayableRecords = showNonPlayableRecords,
                .showNamedRecords = showNamedRecords,
                .showUnnamedRecords = showUnnamedRecords,
                .showDeletedRecords = showDeletedRecords,
                .showUnknownCategories = showUnknownCategories,
                .pluginGlobalSearchMode = pluginGlobalSearchMode,
                .pluginSearchCaseSensitive = pluginSearchCaseSensitive,
                .favoriteForms = favoriteForms,
                .selectedPluginTreeRecordFormID = selectedPluginTreeRecordFormID,
                .recentPluginRecordFormIDs = recentPluginRecordFormIDs,
                .pluginBrowserCacheVersion = pluginBrowserCacheVersion,
                .pluginBrowserCacheSearch = pluginBrowserCacheSearch,
                .pluginBrowserCacheSelectedPlugin = pluginBrowserCacheSelectedPlugin,
                .pluginBrowserCacheShowPlayable = pluginBrowserCacheShowPlayable,
                .pluginBrowserCacheShowNonPlayable = pluginBrowserCacheShowNonPlayable,
                .pluginBrowserCacheShowNamed = pluginBrowserCacheShowNamed,
                .pluginBrowserCacheShowUnnamed = pluginBrowserCacheShowUnnamed,
                .pluginBrowserCacheShowDeleted = pluginBrowserCacheShowDeleted,
                .pluginBrowserCacheShowUnknown = pluginBrowserCacheShowUnknown,
                .pluginBrowserCacheGlobalSearchMode = pluginBrowserCacheGlobalSearchMode,
                .pluginBrowserCacheSearchCaseSensitive = pluginBrowserCacheSearchCaseSensitive,
                .pluginBrowserGroupedRecordsCache = pluginBrowserGroupedRecordsCache,
                .pluginBrowserOrderedPluginsCache = pluginBrowserOrderedPluginsCache,
                .pluginBrowserGlobalSearchResultsCache = pluginBrowserGlobalSearchResultsCache,
                .localize = L,
                .persistListFilters = []() {
                    PersistListFilterSettings();
                },
                .openItemGrantPopup = [](const FormEntry& entry) {
                    OpenItemGrantPopup(entry);
                },
                .openGlobalValuePopup = [](std::uint32_t formID) {
                    OpenGlobalValuePopup(formID);
                },
                .requestActionConfirmation = [](std::string title, std::string message, std::function<void()> callback) {
                    RequestActionConfirmation(std::move(title), std::move(message), std::move(callback));
                }
            };

            PluginBrowserTab::Draw(plugins, cache, dataVersion, context);
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
                auto drawWrappedButton = [](const char* label, bool& firstInRow) {
                    const auto& style = ImGui::GetStyle();
                    const float buttonWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;

                    if (!firstInRow) {
                        const float needed = style.ItemSpacing.x + buttonWidth;
                        if (ImGui::GetContentRegionAvail().x >= needed) {
                            ImGui::SameLine();
                        } else {
                            firstInRow = true;
                        }
                    }

                    const bool pressed = ImGui::Button(label);
                    firstInRow = false;
                    return pressed;
                };

                bool firstActionInRow = true;
                if (drawWrappedButton(L("Items", "sGiveItem", "Give Item"), firstActionInRow)) {
                    OpenItemGrantPopup(*selectedIt);
                }
                if (drawWrappedButton(L("NPCs", "sSpawnAtPlayer", "Spawn At Player"), firstActionInRow)) {
                    FormActions::SpawnAtPlayer(selectedIt->formID, 1);
                }

                const bool isFavorite = favoriteForms.contains(selectedIt->formID);
                if (drawWrappedButton(isFavorite ? L("General", "sRemoveFavorite", "Remove Favorite") : L("General", "sAddFavorite", "Add Favorite"), firstActionInRow)) {
                    if (isFavorite) {
                        favoriteForms.erase(selectedIt->formID);
                    } else {
                        favoriteForms.insert(selectedIt->formID);
                    }
                }

                if (drawWrappedButton(L("General", "sCopyFormID", "Copy FormID"), firstActionInRow)) {
                    FormActions::CopyFormID(selectedIt->formID);
                }
                if (const char* editorID = TryGetEditorID(selectedIt->formID)) {
                    if (drawWrappedButton(L("General", "sCopyEditorID", "Copy EditorID"), firstActionInRow)) {
                        ImGui::SetClipboardText(editorID);
                    }
                }
                if (drawWrappedButton(L("General", "sCopyRecordSource", "Copy Record Source"), firstActionInRow)) {
                    ImGui::SetClipboardText(selectedIt->sourcePlugin.c_str());
                }
            }
        }

        void DrawItemBrowser(const FormCache& cache)
        {
            ItemBrowserTabContext context{
                .selectedPluginFilter = selectedPluginFilter,
                .itemSearch = itemSearch,
                .itemSearchBuffer = itemSearchBuffer,
                .itemSearchBufferSize = sizeof(itemSearchBuffer),
                .searchCaseSensitive = itemSearchCaseSensitive,
                .showPlayableRecords = showPlayableRecords,
                .showNonPlayableRecords = showNonPlayableRecords,
                .showNamedRecords = showNamedRecords,
                .showUnnamedRecords = showUnnamedRecords,
                .showDeletedRecords = showDeletedRecords,
                .itemSort = itemSort,
                .selectedItemRows = selectedItemRows,
                .favoriteForms = favoriteForms,
                .localize = L,
                .drawPluginFilterStatus = []() {
                    DrawPluginFilterStatus();
                },
                .persistListFilters = []() {
                    PersistListFilterSettings();
                },
                .openItemGrantPopup = [](const FormEntry& entry) {
                    OpenItemGrantPopup(entry);
                },
                .tryGetEditorID = [](std::uint32_t formID) {
                    return TryGetEditorID(formID);
                }
            };

            ItemBrowserTab::Draw(cache, context);
        }

        void DrawNPCBrowser(const FormCache& cache)
        {
            NPCBrowserTab::Draw(
                cache,
                npcSearchBuffer,
                sizeof(npcSearchBuffer),
                npcSearch,
                npcSearchCaseSensitive,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                favoriteForms,
                []() {
                    DrawPluginFilterStatus();
                },
                []() {
                    PersistListFilterSettings();
                },
                ApplyLocalRecordFiltersForTabs,
                L);
        }

        void DrawObjectBrowser(const FormCache& cache)
        {
            ObjectBrowserTab::Draw(
                cache,
                objectSearchBuffer,
                sizeof(objectSearchBuffer),
                objectSearch,
                objectSearchCaseSensitive,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                favoriteForms,
                []() {
                    DrawPluginFilterStatus();
                },
                []() {
                    PersistListFilterSettings();
                },
                ApplyLocalRecordFiltersForTabs,
                L);
        }

        void DrawSpellPerkBrowser(const FormCache& cache)
        {
            SpellPerkBrowserTab::Draw(
                cache,
                spellPerkSearchBuffer,
                sizeof(spellPerkSearchBuffer),
                spellPerkSearch,
                spellPerkSearchCaseSensitive,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                favoriteForms,
                []() {
                    DrawPluginFilterStatus();
                },
                []() {
                    PersistListFilterSettings();
                },
                ApplyLocalRecordFiltersForTabs,
                L);
        }
    }

    void MainWindow::Draw()
    {
        EnsureFavoritesLoaded();
        const auto favoritesBefore = favoriteForms;

        if (refreshDataRequested && !refreshDataInProgress) {
            refreshDataInProgress = true;
            DataManager::Refresh();
            refreshDataInProgress = false;
            refreshDataRequested = false;
        }

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

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && FormActions::CanUndoLastAction()) {
                FormActions::UndoLastAction();
            }

            const auto& style = ImGui::GetStyle();
            const float footerHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y + style.WindowPadding.y + 8.0f;
            if (ImGui::BeginChild("MainContentRegion", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::BeginTabBar("MainTabs")) {
                    if (ImGui::BeginTabItem(L("PluginBrowser", "sBrowserTab", "Plugin Browser"))) {
                        activeMainTab = "Plugin Browser";

                        if (refreshDataInProgress) {
                            ImGui::BeginDisabled(true);
                            ImGui::Button(L("General", "sRefreshData", "Refresh Data"));
                            ImGui::EndDisabled();
                        } else {
                            if (ImGui::Button(L("General", "sRefreshData", "Refresh Data"))) {
                                refreshDataRequested = true;
                            }
                        }

                        if (refreshDataRequested || refreshDataInProgress) {
                            ImGui::SameLine();
                            ImGui::TextUnformatted(L("General", "sRefreshingData", "Refreshing..."));
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
                        DrawObjectBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(spellPerkTabLabel)) {
                        activeMainTab = "Spells & Perks";
                        ImGui::Text("%s: %zu  %s: %zu", L("Spells", "sSpells", "Spells"), counts.spells, L("Spells", "sPerks", "Perks"), counts.perks);
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

                    if (ImGui::BeginTabItem(L("Logs", "sTabName", "Logs"))) {
                        activeMainTab = "Logs";
                        LogViewerTab::Draw(L);
                        ImGui::EndTabItem();
                    }

                    RenderItemGrantPopup();
                    RenderConfirmActionPopup();
                    RenderGlobalValuePopup();

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

                const float resetWidth = CalcButtonWidth(L("General", "sResetFilters", "Reset Filters"));
                const float undoWidth = CalcButtonWidth(L("General", "sUndoLastAction", "Undo Last Action"));
                const float actionsWidth = resetWidth + style.ItemSpacing.x + undoWidth;
                const float actionStartX = ImGui::GetWindowContentRegionMax().x - actionsWidth;
                if (actionStartX > ImGui::GetCursorPosX()) {
                    ImGui::SameLine(actionStartX);
                } else {
                    ImGui::SameLine();
                }

                if (ImGui::Button(L("General", "sResetFilters", "Reset Filters"))) {
                    ResetQuickFilters();
                }
                ImGui::SameLine();
                const bool canUndo = FormActions::CanUndoLastAction();
                if (!canUndo) {
                    ImGui::BeginDisabled(true);
                }
                if (ImGui::Button(L("General", "sUndoLastAction", "Undo Last Action")) && canUndo) {
                    FormActions::UndoLastAction();
                }
                if (!canUndo) {
                    ImGui::EndDisabled();
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
