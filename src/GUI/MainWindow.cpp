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
#include "Input/GamepadInput.h"
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
#include <array>
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

        std::vector<FormEntry> ApplyLocalRecordFilters(const std::vector<FormEntry>& entries);

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

        void PersistFilterCheckboxSettings()
        {
            auto& settings = Config::GetMutable();
            settings.listShowPlayable = showPlayableRecords;
            settings.listShowNonPlayable = showNonPlayableRecords;
            settings.listShowNamed = showNamedRecords;
            settings.listShowUnnamed = showUnnamedRecords;
            settings.listShowDeleted = showDeletedRecords;
            settings.pluginGlobalSearchMode = pluginGlobalSearchMode;
            settings.pluginShowUnknownCategories = showUnknownCategories;
            settings.pluginSearchCaseSensitive = pluginSearchCaseSensitive;
            settings.itemSearchCaseSensitive = itemSearchCaseSensitive;
            settings.npcSearchCaseSensitive = npcSearchCaseSensitive;
            settings.objectSearchCaseSensitive = objectSearchCaseSensitive;
            settings.spellPerkSearchCaseSensitive = spellPerkSearchCaseSensitive;
            Config::Save();
        }

        std::vector<FormEntry> ApplyLocalRecordFiltersForTabs(const std::vector<FormEntry>& entries)
        {
            return ApplyLocalRecordFilters(entries);
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
            pluginGlobalSearchMode = settings.pluginGlobalSearchMode;
            showUnknownCategories = settings.pluginShowUnknownCategories;
            pluginSearchCaseSensitive = settings.pluginSearchCaseSensitive;
            itemSearchCaseSensitive = settings.itemSearchCaseSensitive;
            npcSearchCaseSensitive = settings.npcSearchCaseSensitive;
            objectSearchCaseSensitive = settings.objectSearchCaseSensitive;
            spellPerkSearchCaseSensitive = settings.spellPerkSearchCaseSensitive;
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
            itemGrantPopup.includeAmmo = (entry.category == "Weapon" || entry.category == "Weapons" || entry.category == "WEAP") && itemGrantPopup.ammoFormID != 0;
            itemGrantPopup.openRequested = true;
            itemGrantPopup.visible = true;
        }

        void RenderItemGrantPopup()
        {
            if (itemGrantPopup.openRequested) {
                ImGui::OpenPopup(L("Items", "sGivePopupTitle", "Add Item"));
                itemGrantPopup.openRequested = false;
            }

            ImGui::SetNextWindowSize(ImVec2(480.0f, 340.0f), ImGuiCond_Appearing);
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
            ImGui::TextDisabled("%08X  |  %s", itemGrantPopup.entry.formID, itemGrantPopup.entry.sourcePlugin.c_str());

            ImGui::Separator();

            ImGui::TextUnformatted(L("Items", "sQuantity", "Quantity"));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragInt("##ItemQtyDrag", &itemGrantPopup.quantity, 0.25f, 1, 9999, "%d");
            if (itemGrantPopup.quantity < 1) {
                itemGrantPopup.quantity = 1;
            }

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
            ImGui::SameLine();
            if (ImGui::Button("1000##ItemQtyPreset")) {
                itemGrantPopup.quantity = 1000;
            }

            const bool hasAmmoOption = itemGrantPopup.ammoFormID != 0;
            if (hasAmmoOption) {
                ImGui::Spacing();
                ImGui::Checkbox(L("Player", "sIncludeAmmo", "Include Ammo"), &itemGrantPopup.includeAmmo);
            }

            if (hasAmmoOption && itemGrantPopup.includeAmmo) {
                ImGui::Separator();
                ImGui::TextUnformatted(L("Items", "sAmmoQuantity", "Ammo Quantity"));
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragInt("##AmmoQtyDrag", &itemGrantPopup.ammoQuantity, 1.0f, 0, 50000, "%d");
                if (itemGrantPopup.ammoQuantity < 0) {
                    itemGrantPopup.ammoQuantity = 0;
                }

                if (ImGui::Button("0##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("50##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 50;
                }
                ImGui::SameLine();
                if (ImGui::Button("100##AmmoQtyPreset")) {
                    itemGrantPopup.ammoQuantity = 100;
                }
            }

            ImGui::Separator();
            const char* giveToPlayerLabel = L("Items", "sGiveToPlayer", "Give To Player");
            const char* cancelLabel = L("General", "sCancel", "Cancel");
            const float popupButtonMinWidth = 96.0f;

            const bool canApply = itemGrantPopup.quantity > 0;
            if (!canApply) {
                ImGui::BeginDisabled(true);
            }
            const bool applyPressed = ImGui::Button(giveToPlayerLabel, ImVec2(CalcButtonWidth(giveToPlayerLabel, popupButtonMinWidth), 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter);
            if (!canApply) {
                ImGui::EndDisabled();
            }
            if (applyPressed && canApply) {
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

        std::vector<FormEntry> ApplyLocalRecordFilters(const std::vector<FormEntry>& entries)
        {
            std::vector<FormEntry> filtered;
            filtered.reserve(entries.size());

            for (const auto& entry : entries) {
                if (PassesLocalRecordFilters(entry)) {
                    filtered.push_back(entry);
                }
            }

            return filtered;
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
                .equipWeaponAmmoCount = playerCurrentWeaponAmmoAmount,
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
                .persistFilterCheckboxes = []() {
                    PersistFilterCheckboxSettings();
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
                .persistFilterCheckboxes = []() {
                    PersistFilterCheckboxSettings();
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
                []() {
                    PersistFilterCheckboxSettings();
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
                []() {
                    PersistFilterCheckboxSettings();
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
                []() {
                    PersistFilterCheckboxSettings();
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
                    std::string requestedTab{};
                    if (Config::Get().enableGamepadNav && (GamepadInput::WasTabNextPressed() || GamepadInput::WasTabPrevPressed())) {
                        constexpr std::array<std::string_view, 8> tabOrder{
                            "Plugin Browser",
                            "Player",
                            "Item Browser",
                            "NPC Browser",
                            "Object Browser",
                            "Spells & Perks",
                            "Settings",
                            "Logs"
                        };

                        std::size_t currentIndex = 0;
                        for (std::size_t i = 0; i < tabOrder.size(); ++i) {
                            if (activeMainTab == tabOrder[i]) {
                                currentIndex = i;
                                break;
                            }
                        }

                        if (GamepadInput::WasTabNextPressed()) {
                            currentIndex = (currentIndex + 1) % tabOrder.size();
                        } else {
                            currentIndex = (currentIndex + tabOrder.size() - 1) % tabOrder.size();
                        }
                        requestedTab = tabOrder[currentIndex];
                    }

                    if (ImGui::BeginTabItem(L("PluginBrowser", "sBrowserTab", "Plugin Browser"), nullptr,
                        requestedTab == "Plugin Browser" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
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
                        ImGui::TextDisabled("|");
                        ImGui::SameLine();
                        DrawPluginFilterStatus();

                        DrawPluginBrowser(plugins, cache, dataVersion);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(L("Player", "sTabName", "Player"), nullptr,
                        requestedTab == "Player" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                        activeMainTab = "Player";
                        DrawPlayerTab(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(itemTabLabel, nullptr,
                        requestedTab == "Item Browser" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                        activeMainTab = "Item Browser";
                        DrawItemBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(npcTabLabel, nullptr,
                        requestedTab == "NPC Browser" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                        activeMainTab = "NPC Browser";
                        DrawNPCBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(objectTabLabel, nullptr,
                        requestedTab == "Object Browser" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                        activeMainTab = "Object Browser";
                        DrawObjectBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(spellPerkTabLabel, nullptr,
                        requestedTab == "Spells & Perks" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                        activeMainTab = "Spells & Perks";
                        DrawSpellPerkBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const auto settingsTabName = Language::Get("Settings", "sTabName");
                    const auto* settingsLabel = settingsTabName.empty() ? "Settings" : settingsTabName.data();
                    if (ImGui::BeginTabItem(settingsLabel, nullptr,
                        requestedTab == "Settings" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                        activeMainTab = "Settings";
                        SettingsTab::Draw();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem(L("Logs", "sTabName", "Logs"), nullptr,
                        requestedTab == "Logs" ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
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

                if (settings.showFPSInStatus) {
                    ImGui::Text("%s: %zu  %s: %zu  %s: %zu  |  %.0f %s  %.1fms",
                        L("PluginBrowser", "sPluginsCount", "Plugins"),
                        plugins.size(),
                        L("General", "sForms", "Forms"),
                        totalForms,
                        L("General", "sFavorites", "Favorites"),
                        favoriteForms.size(),
                        fps, L("General", "sFPS", "FPS"), frameTime);
                } else {
                    ImGui::Text("%s: %zu  %s: %zu  %s: %zu",
                        L("PluginBrowser", "sPluginsCount", "Plugins"),
                        plugins.size(),
                        L("General", "sForms", "Forms"),
                        totalForms,
                        L("General", "sFavorites", "Favorites"),
                        favoriteForms.size());
                }

                if (GamepadInput::IsGamepadConnected()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::TextDisabled("[Gamepad]");
                }

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
