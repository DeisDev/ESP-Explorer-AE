#include "GUI/MainWindow.h"

#include "Config/Config.h"
#include "Data/DataManager.h"
#include "Filters/AdvancedRecordFilters.h"
#include "GUI/Tabs/ItemBrowserTab.h"
#include "GUI/Tabs/InventoryTab.h"
#include "GUI/Tabs/LogViewerTab.h"
#include "GUI/Tabs/NPCBrowserTab.h"
#include "GUI/Tabs/CellBrowserTab.h"
#include "GUI/Tabs/ObjectBrowserTab.h"
#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Tabs/SettingsTab.h"
#include "GUI/Tabs/SpellPerkBrowserTab.h"
#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/ItemGrantPopup.h"
#include "GUI/Widgets/MainWindowPopups.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"
#include "Hooks/Hooks.h"
#include "Input/GamepadInput.h"
#include "Localization/Language.h"
#include "Logging/Logger.h"

#include <imgui.h>

#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESNPC.h>

#include <RE/A/ActorValue.h>
#include <RE/P/PlayerCharacter.h>
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
        constexpr auto kStartupTabLastActive = "__last__";
        constexpr std::array<std::string_view, 9> kMainTabOrder{
            "Plugin Browser",
            "Inventory",
            "Item Browser",
            "NPC Browser",
            "Cell Browser",
            "Object Browser",
            "Spells & Perks",
            "Settings",
            "Logs"
        };

        std::string pluginSearch{};
        std::string itemSearch{};
        std::string selectedPluginFilter{};
        std::string selectedPluginDiagnostics{};
        ItemSortState itemSort{};

        char pluginSearchBuffer[256]{};
        char itemSearchBuffer[256]{};
        char npcSearchBuffer[256]{};
        char objectSearchBuffer[256]{};
        char spellPerkSearchBuffer[256]{};
        char cellSearchBuffer[256]{};

        std::string npcSearch{};
        std::string objectSearch{};
        std::string spellPerkSearch{};
        std::string cellSearch{};
        std::unordered_set<std::uint32_t> favoriteForms{};
        bool favoritesInitialized{ false };
        std::string activeMainTab{};
        std::string previousMainTab{};
        std::string requestedMainTab{};
        bool tabSearchFocusPending{ false };
        bool collapseSelectedRecordDiagnostics{ false };

        bool playerGodModeEnabled{ false };
        bool playerNoClipEnabled{ false };
        int playerCurrentWeaponAmmoAmount{ 200 };
        int playerAllAmmoAmount{ 100 };
        int playerPerkPointsAmount{ 1 };
        int playerLevelAmount{ 1 };
        float playerTimeOfDay{ 12.0f };
        std::uint32_t selectedPluginTreeRecordFormID{ 0 };
        std::unordered_set<std::uint32_t> selectedPluginTreeRecordFormIDs{};
        std::uint32_t pluginTreeLastClickedFormID{ 0 };
        bool showPlayableRecords{ true };
        bool showNonPlayableRecords{ false };
        bool showNamedRecords{ true };
        bool showUnnamedRecords{ false };
        bool showDeletedRecords{ false };
        std::vector<AdvancedFilterRule> advancedRecordFilters{};
        std::unordered_set<std::string> hiddenPlugins{};
        std::uint64_t advancedRecordFilterRevision{ 1 };
        bool showUnknownCategories{ false };
        bool pluginGlobalSearchMode{ false };
        std::unordered_map<std::string, std::uint32_t> selectedItemRows{};
        std::uint64_t pluginBrowserCacheVersion{ 0 };
        std::string pluginBrowserCacheSearch{};
        std::string pluginBrowserCacheSelectedPlugin{};
        bool pluginBrowserCacheShowPlayable{ true };
        bool pluginBrowserCacheShowNonPlayable{ false };
        bool pluginBrowserCacheShowNamed{ true };
        bool pluginBrowserCacheShowUnnamed{ false };
        bool pluginBrowserCacheShowDeleted{ false };
        std::uint64_t pluginBrowserCacheAdvancedFilterRevision{ 0 };
        bool pluginBrowserCacheShowUnknown{ false };
        bool pluginBrowserCacheGlobalSearchMode{ false };
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<const FormEntry*>>> pluginBrowserGroupedRecordsCache{};
        std::vector<std::string> pluginBrowserOrderedPluginsCache{};
        std::vector<const FormEntry*> pluginBrowserGlobalSearchResultsCache{};
        std::deque<std::uint32_t> recentPluginRecordFormIDs{};
        bool refreshDataRequested{ false };
        bool refreshDataInProgress{ false };

        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        }

        void ResetQuickFilters()
        {
            selectedPluginFilter.clear();
            selectedPluginDiagnostics.clear();
            pluginSearch.clear();
            itemSearch.clear();
            npcSearch.clear();
            objectSearch.clear();
            spellPerkSearch.clear();
            cellSearch.clear();

            pluginSearchBuffer[0] = '\0';
            itemSearchBuffer[0] = '\0';
            npcSearchBuffer[0] = '\0';
            objectSearchBuffer[0] = '\0';
            spellPerkSearchBuffer[0] = '\0';
            cellSearchBuffer[0] = '\0';
        }

        void DrawPluginFilterStatus()
        {
            ImGui::Text("%s: %s", L("PluginBrowser", "sFilter", "Filter"), selectedPluginFilter.empty() ? L("General", "sNone", "None") : selectedPluginFilter.c_str());
        }

        void TrackRecentRecord(std::uint32_t formID)
        {
            if (formID == 0) {
                return;
            }

            const std::size_t maxRecentRecords = static_cast<std::size_t>((std::clamp)(Config::Get().recentRecordsLimit, 5, 100));
            recentPluginRecordFormIDs.erase(
                std::remove(recentPluginRecordFormIDs.begin(), recentPluginRecordFormIDs.end(), formID),
                recentPluginRecordFormIDs.end());
            recentPluginRecordFormIDs.push_front(formID);

            while (recentPluginRecordFormIDs.size() > maxRecentRecords) {
                recentPluginRecordFormIDs.pop_back();
            }
        }

        float CalcButtonWidth(const char* label, float minimumWidth = 0.0f)
        {
            const auto& style = ImGui::GetStyle();
            const float width = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x;
            return (std::max)(width, minimumWidth);
        }

        ImVec2 ClampWindowSizeToViewport(const ImVec2& requestedSize, const ImVec2& minimumSize, const ImVec2& maximumSize)
        {
            return ImVec2(
                std::clamp(requestedSize.x, minimumSize.x, maximumSize.x),
                std::clamp(requestedSize.y, minimumSize.y, maximumSize.y));
        }

        ImVec2 ClampWindowPosToViewport(const ImVec2& requestedPos, const ImVec2& windowSize, const ImVec2& viewportPos, const ImVec2& viewportSize)
        {
            const float maxX = (std::max)(viewportPos.x, viewportPos.x + viewportSize.x - windowSize.x);
            const float maxY = (std::max)(viewportPos.y, viewportPos.y + viewportSize.y - windowSize.y);
            return ImVec2(
                std::clamp(requestedPos.x, viewportPos.x, maxX),
                std::clamp(requestedPos.y, viewportPos.y, maxY));
        }

        void DrawActionHistoryPopup()
        {
            ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
            const auto& settings = Config::Get();
            ImGui::SetNextWindowPos(ImVec2(settings.actionHistoryWindowX, settings.actionHistoryWindowY), ImGuiCond_Appearing);
            ImVec4 popupBg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
            popupBg.x *= 0.78f;
            popupBg.y *= 0.80f;
            popupBg.z *= 0.82f;
            popupBg.w = (std::max)(popupBg.w, 0.96f);
            ImVec4 popupBorder = ImGui::GetStyleColorVec4(ImGuiCol_Border);
            popupBorder.x = (std::min)(popupBorder.x + 0.20f, 1.0f);
            popupBorder.y = (std::min)(popupBorder.y + 0.20f, 1.0f);
            popupBorder.z = (std::min)(popupBorder.z + 0.20f, 1.0f);
            popupBorder.w = 1.0f;
            ImGui::PushStyleColor(ImGuiCol_PopupBg, popupBg);
            ImGui::PushStyleColor(ImGuiCol_Border, popupBorder);
            if (!ImGui::BeginPopup("##ActionHistoryPopup")) {
                ImGui::PopStyleColor(2);
                return;
            }
            ImGui::PopStyleColor(2);

            auto& mutableSettings = Config::GetMutable();
            const ImVec2 popupPos = ImGui::GetWindowPos();
            if (std::fabs(mutableSettings.actionHistoryWindowX - popupPos.x) > 0.5f ||
                std::fabs(mutableSettings.actionHistoryWindowY - popupPos.y) > 0.5f) {
                mutableSettings.actionHistoryWindowX = popupPos.x;
                mutableSettings.actionHistoryWindowY = popupPos.y;
                Config::RequestSave();
            }

            const auto history = FormActions::GetRecentActionHistory();
            const auto& style = ImGui::GetStyle();

            ImGui::TextUnformatted(L("General", "sActionHistory", "Action History"));
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu)", history.size());
            const float closeButtonWidth = 28.0f;
            ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - closeButtonWidth));
            if (ImGui::Button("X", ImVec2(closeButtonWidth, 0.0f))) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (history.empty()) {
                ImGui::TextDisabled("%s", L("General", "sNoRecentActions", "No recent actions yet."));
                ImGui::EndPopup();
                return;
            }

            const float listHeight = (std::max)(280.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing());
            if (ImGui::BeginChild("##ActionHistoryList", ImVec2(0.0f, listHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                const float undoButtonWidth = 112.0f;
                const float minCardHeight = ImGui::GetFrameHeightWithSpacing() * 3.0f;
                ImVec4 evenCardBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
                evenCardBg.x *= 0.82f;
                evenCardBg.y *= 0.84f;
                evenCardBg.z *= 0.88f;
                evenCardBg.w = (std::max)(evenCardBg.w, 0.95f);
                ImVec4 oddCardBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);
                oddCardBg.x *= 0.88f;
                oddCardBg.y *= 0.90f;
                oddCardBg.z *= 0.94f;
                oddCardBg.w = (std::max)(oddCardBg.w, 0.92f);
                ImVec4 cardBorder = ImGui::GetStyleColorVec4(ImGuiCol_Border);
                cardBorder.x = (std::min)(cardBorder.x + 0.15f, 1.0f);
                cardBorder.y = (std::min)(cardBorder.y + 0.15f, 1.0f);
                cardBorder.z = (std::min)(cardBorder.z + 0.15f, 1.0f);
                cardBorder.w = 1.0f;

                for (std::size_t index = 0; index < history.size(); ++index) {
                    const auto& entry = history[index];
                    ImGui::PushID(static_cast<int>(entry.id));

                    const float availableWidth = ImGui::GetContentRegionAvail().x;
                    const float wrapWidth = (std::max)(160.0f, availableWidth - undoButtonWidth - style.ItemSpacing.x * 3.0f - style.WindowPadding.x * 2.0f);
                    const ImVec2 descriptionSize = ImGui::CalcTextSize(entry.description.c_str(), nullptr, false, wrapWidth);
                    const float cardHeight = (std::max)(minCardHeight, descriptionSize.y + style.WindowPadding.y * 2.0f + ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y * 2.0f);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, (index % 2 == 0) ? evenCardBg : oddCardBg);
                    ImGui::PushStyleColor(ImGuiCol_Border, cardBorder);
                    ImGui::BeginChild("##ActionHistoryEntry", ImVec2(0.0f, cardHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    ImGui::PopStyleColor(2);

                    char indexLabel[16]{};
                    std::snprintf(indexLabel, sizeof(indexLabel), "#%02zu", index + 1);
                    ImGui::TextDisabled("%s", indexLabel);
                    ImGui::SameLine();
                    if (entry.canUndo) {
                        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), "%s", L("General", "sUndo", "Undo"));
                    } else {
                        ImGui::TextDisabled("%s", L("General", "sNoUndoAvailable", "No Undo"));
                    }

                    const float buttonX = ImGui::GetWindowContentRegionMax().x - undoButtonWidth;
                    ImGui::SetCursorPos(ImVec2(buttonX, style.WindowPadding.y));
                    if (entry.canUndo) {
                        if (ImGui::Button(L("General", "sUndo", "Undo"), ImVec2(undoButtonWidth, 0.0f))) {
                            FormActions::UndoAction(entry.id);
                        }
                    } else {
                        ImGui::BeginDisabled(true);
                        ImGui::Button(L("General", "sNoUndoAvailable", "No Undo"), ImVec2(undoButtonWidth, 0.0f));
                        ImGui::EndDisabled();
                    }

                    ImGui::SetCursorPos(ImVec2(style.WindowPadding.x, style.WindowPadding.y + ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y * 0.5f));
                    ImGui::PushTextWrapPos(buttonX - style.ItemSpacing.x);
                    ImGui::TextUnformatted(entry.description.c_str());
                    ImGui::PopTextWrapPos();

                    ImGui::EndChild();
                    ImGui::PopID();
                }
            }

            ImGui::EndChild();

            ImGui::EndPopup();
        }

        void DrawWaitingForDataHandlerPopup()
        {
            const char* title = L("General", "sWindowTitle", "ESP Explorer AE");
            const char* message = L("General", "sWaitingForDataHandler", "Waiting for data handler to initialize");

            const float popupScale = (std::clamp)(Config::Get().fontSize / 20.0f, 0.75f, 1.5f);
            const float messageWidth = ImGui::CalcTextSize(message).x;
            const float popupWidth = (std::max)(360.0f * popupScale, messageWidth + ImGui::GetStyle().WindowPadding.x * 2.0f);
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 center = viewport ?
                                      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f) :
                                      ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(popupWidth, 0.0f), ImGuiCond_Always);

            const std::string windowId = std::string(title) + "##WaitingForDataHandlerPopup";
            if (ImGui::Begin(
                    windowId.c_str(),
                    nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::TextWrapped("%s", message);
            }
            ImGui::End();
        }

        bool PassesLocalRecordFilters(const FormEntry& entry);

        struct MainTabLabelsCache
        {
            std::uint64_t dataVersion{ (std::numeric_limits<std::uint64_t>::max)() };
            std::string languageCode;
            bool cachedShowPlayable{ true };
            bool cachedShowNonPlayable{ false };
            bool cachedShowNamed{ true };
            bool cachedShowUnnamed{ false };
            bool cachedShowDeleted{ false };
            std::uint64_t cachedAdvFilterRevision{ 0 };
            std::string pluginLabel;
            std::string inventoryLabel;
            std::string itemLabel;
            std::string npcLabel;
            std::string cellLabel;
            std::string objectLabel;
            std::string spellPerkLabel;
            std::string settingsLabel;
            std::string logsLabel;
        };

        std::size_t CountFiltered(const std::vector<FormEntry>& entries)
        {
            std::size_t count = 0;
            for (const auto& entry : entries) {
                if (PassesLocalRecordFilters(entry)) {
                    ++count;
                }
            }
            return count;
        }

        void FormatTabLabel(char* buf, std::size_t bufSize, const char* label, std::size_t filtered, std::size_t total, const char* stableId)
        {
            if (filtered < total) {
                std::snprintf(buf, bufSize, "%s (%zu / %zu)###%s", label, filtered, total, stableId);
            } else {
                std::snprintf(buf, bufSize, "%s (%zu)###%s", label, total, stableId);
            }
        }

        const MainTabLabelsCache& GetMainTabLabels(const FormCache& formCache, const FormCategoryCounts& counts, std::uint64_t dataVersion)
        {
            static MainTabLabelsCache cache;
            const auto currentLanguage = Language::GetCurrentLanguageCode();

            const bool needsRebuild =
                cache.dataVersion != dataVersion ||
                cache.languageCode != currentLanguage ||
                cache.cachedShowPlayable != showPlayableRecords ||
                cache.cachedShowNonPlayable != showNonPlayableRecords ||
                cache.cachedShowNamed != showNamedRecords ||
                cache.cachedShowUnnamed != showUnnamedRecords ||
                cache.cachedShowDeleted != showDeletedRecords ||
                cache.cachedAdvFilterRevision != advancedRecordFilterRevision;

            if (!needsRebuild) {
                return cache;
            }

            cache.dataVersion = dataVersion;
            cache.languageCode = currentLanguage;
            cache.cachedShowPlayable = showPlayableRecords;
            cache.cachedShowNonPlayable = showNonPlayableRecords;
            cache.cachedShowNamed = showNamedRecords;
            cache.cachedShowUnnamed = showUnnamedRecords;
            cache.cachedShowDeleted = showDeletedRecords;
            cache.cachedAdvFilterRevision = advancedRecordFilterRevision;

            const bool filtersActive = !showPlayableRecords || showNonPlayableRecords || !showNamedRecords || showUnnamedRecords || showDeletedRecords || advancedRecordFilterRevision > 1;

            char labelBuffer[120]{};

            cache.pluginLabel = L("PluginBrowser", "sBrowserTab", "Plugin Browser");
            cache.inventoryLabel = L("Inventory", "sTabName", "Inventory");

            const std::size_t totalItems = counts.weapons + counts.armors + counts.ammo + counts.misc;
            if (filtersActive) {
                const std::size_t filteredItems = CountFiltered(formCache.weapons) + CountFiltered(formCache.armors) + CountFiltered(formCache.ammo) + CountFiltered(formCache.misc);
                FormatTabLabel(labelBuffer, sizeof(labelBuffer), L("Items", "sBrowserTab", "Item Browser"), filteredItems, totalItems, "MainTabItem");
            } else {
                std::snprintf(labelBuffer, sizeof(labelBuffer), "%s (%zu)###MainTabItem", L("Items", "sBrowserTab", "Item Browser"), totalItems);
            }
            cache.itemLabel = labelBuffer;

            if (filtersActive) {
                const std::size_t filteredNpcs = CountFiltered(formCache.npcs);
                FormatTabLabel(labelBuffer, sizeof(labelBuffer), L("NPCs", "sBrowserTab", "NPC Browser"), filteredNpcs, counts.npcs, "MainTabNPC");
            } else {
                std::snprintf(labelBuffer, sizeof(labelBuffer), "%s (%zu)###MainTabNPC", L("NPCs", "sBrowserTab", "NPC Browser"), counts.npcs);
            }
            cache.npcLabel = labelBuffer;

            if (filtersActive) {
                const std::size_t filteredCells = CountFiltered(formCache.cells);
                FormatTabLabel(labelBuffer, sizeof(labelBuffer), L("Cells", "sBrowserTab", "Cell Browser"), filteredCells, counts.cells, "MainTabCell");
            } else {
                std::snprintf(labelBuffer, sizeof(labelBuffer), "%s (%zu)###MainTabCell", L("Cells", "sBrowserTab", "Cell Browser"), counts.cells);
            }
            cache.cellLabel = labelBuffer;

            const std::size_t totalObjects = counts.activators + counts.containers + counts.statics + counts.furniture;
            if (filtersActive) {
                const std::size_t filteredObjects = CountFiltered(formCache.activators) + CountFiltered(formCache.containers) + CountFiltered(formCache.statics) + CountFiltered(formCache.furniture);
                FormatTabLabel(labelBuffer, sizeof(labelBuffer), L("Objects", "sBrowserTab", "Object Browser"), filteredObjects, totalObjects, "MainTabObject");
            } else {
                std::snprintf(labelBuffer, sizeof(labelBuffer), "%s (%zu)###MainTabObject", L("Objects", "sBrowserTab", "Object Browser"), totalObjects);
            }
            cache.objectLabel = labelBuffer;

            const std::size_t totalSpellPerks = counts.spells + counts.perks;
            if (filtersActive) {
                const std::size_t filteredSpellPerks = CountFiltered(formCache.spells) + CountFiltered(formCache.perks);
                FormatTabLabel(labelBuffer, sizeof(labelBuffer), L("Spells", "sBrowserTab", "Spells & Perks"), filteredSpellPerks, totalSpellPerks, "MainTabSpells");
            } else {
                std::snprintf(labelBuffer, sizeof(labelBuffer), "%s (%zu)###MainTabSpells", L("Spells", "sBrowserTab", "Spells & Perks"), totalSpellPerks);
            }
            cache.spellPerkLabel = labelBuffer;

            cache.settingsLabel = L("Settings", "sTabName", "Settings");
            cache.logsLabel = L("Logs", "sTabName", "Logs");

            return cache;
        }

        std::size_t GetVisibleTabCount(bool showLogsTab)
        {
            return showLogsTab ? kMainTabOrder.size() : (kMainTabOrder.size() - 1);
        }

        std::vector<FormEntry> ApplyLocalRecordFilters(const std::vector<FormEntry>& entries);

        void PersistListFilterSettings()
        {
            ++advancedRecordFilterRevision;
            auto& settings = Config::GetMutable();
            settings.listShowNonPlayable = showNonPlayableRecords;
            settings.listShowUnnamed = showUnnamedRecords;
            settings.listShowDeleted = showDeletedRecords;
            settings.advancedRecordFilters = AdvancedRecordFilters::SaveRules(advancedRecordFilters);
            settings.hiddenPlugins = AdvancedRecordFilters::SaveHiddenPlugins(hiddenPlugins);
            Config::RequestSave();
        }

        void PersistFilterCheckboxSettings()
        {
            auto& settings = Config::GetMutable();
            settings.listShowNonPlayable = showNonPlayableRecords;
            settings.listShowUnnamed = showUnnamedRecords;
            settings.listShowDeleted = showDeletedRecords;
            settings.advancedRecordFilters = AdvancedRecordFilters::SaveRules(advancedRecordFilters);
            settings.hiddenPlugins = AdvancedRecordFilters::SaveHiddenPlugins(hiddenPlugins);
            settings.pluginGlobalSearchMode = pluginGlobalSearchMode;
            settings.pluginShowUnknownCategories = showUnknownCategories;
            Config::RequestSave();
        }

        std::vector<FormEntry> ApplyLocalRecordFiltersForTabs(const std::vector<FormEntry>& entries)
        {
            return ApplyLocalRecordFilters(entries);
        }

        std::string ResolveStartupTab(const Settings& settings)
        {
            if (settings.startupTab == kStartupTabLastActive) {
                if (!settings.lastActiveTab.empty()) {
                    return settings.lastActiveTab == "Player" ? "Inventory" : settings.lastActiveTab;
                }
                return "Plugin Browser";
            }

            if (!settings.startupTab.empty()) {
                return settings.startupTab == "Player" ? "Inventory" : settings.startupTab;
            }

            return "Plugin Browser";
        }

        void EnsureFavoritesLoaded()
        {
            if (favoritesInitialized) {
                return;
            }

            const auto& settings = Config::Get();
            favoriteForms.clear();
            favoriteForms.insert(settings.favorites.begin(), settings.favorites.end());
            activeMainTab = ResolveStartupTab(settings);
            if (!settings.showLogsTab && activeMainTab == "Logs") {
                activeMainTab = "Plugin Browser";
            }
            showPlayableRecords = true;
            showNonPlayableRecords = settings.listShowNonPlayable;
            showNamedRecords = true;
            showUnnamedRecords = settings.listShowUnnamed;
            showDeletedRecords = settings.listShowDeleted;
            advancedRecordFilters = AdvancedRecordFilters::LoadRules(settings.advancedRecordFilters);
            hiddenPlugins = AdvancedRecordFilters::LoadHiddenPlugins(settings.hiddenPlugins);
            ++advancedRecordFilterRevision;
            pluginGlobalSearchMode = settings.pluginGlobalSearchMode;
            showUnknownCategories = settings.pluginShowUnknownCategories;
            favoritesInitialized = true;
        }

        void PersistFavoriteForms()
        {
            auto& settings = Config::GetMutable();
            settings.favorites.assign(favoriteForms.begin(), favoriteForms.end());
            Config::RequestSave();
        }

        const char* TryGetEditorID(std::uint32_t formID);

        bool MatchesPluginSearch(const FormEntry& entry, std::string_view query, bool caseSensitive)
        {
            if (query.empty()) {
                return true;
            }

            const std::string formIDText = FormatUtils::FormID(entry.formID);

            if (SharedUtils::ContainsByMode(entry.name, query, caseSensitive) ||
                SharedUtils::ContainsByMode(entry.category, query, caseSensitive) ||
                SharedUtils::ContainsByMode(entry.sourcePlugin, query, caseSensitive) ||
                SharedUtils::ContainsByMode(formIDText, query, caseSensitive)) {
                return true;
            }

            const char* editorID = TryGetEditorID(entry.formID);
            return editorID && SharedUtils::ContainsByMode(editorID, query, caseSensitive);
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
            if (!FormActions::AreGameplayActionsAllowed()) {
                return;
            }

            ItemGrantPopup::Open(entry);
        }

        void OpenItemGrantPopupMultiple(const std::vector<FormEntry>& entries)
        {
            if (!FormActions::AreGameplayActionsAllowed()) {
                return;
            }

            ItemGrantPopup::Open(entries);
        }

        void DrawInventoryTab(const FormCache& cache)
        {
            InventoryTabContext context{
                .localize = L,
                .playerGodModeEnabled = playerGodModeEnabled,
                .playerNoClipEnabled = playerNoClipEnabled,
                .playerCurrentWeaponAmmoAmount = playerCurrentWeaponAmmoAmount,
                .playerAllAmmoAmount = playerAllAmmoAmount,
                .playerPerkPointsAmount = playerPerkPointsAmount,
                .playerLevelAmount = playerLevelAmount,
                .playerTimeOfDay = playerTimeOfDay,
                .cache = cache,
                .searchFocusPending = &tabSearchFocusPending,
                .openItemGrantPopup = [](const FormEntry& entry) {
                    OpenItemGrantPopup(entry);
                },
                .inspectFormInPluginBrowser = [](std::uint32_t formID) {
                    const std::string formIDText = FormatUtils::FormID(formID);
                    requestedMainTab = "Plugin Browser";
                    selectedPluginFilter.clear();
                    selectedPluginDiagnostics.clear();
                    pluginGlobalSearchMode = true;
                    pluginSearch = formIDText;
                    std::snprintf(pluginSearchBuffer, sizeof(pluginSearchBuffer), "%s", formIDText.c_str());
                    selectedPluginTreeRecordFormID = formID;
                    selectedPluginTreeRecordFormIDs.clear();
                    selectedPluginTreeRecordFormIDs.insert(formID);
                    TrackRecentRecord(formID);
                }
            };
            InventoryTab::Draw(context);
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
            if (!hiddenPlugins.empty() && hiddenPlugins.contains(entry.sourcePlugin)) {
                return false;
            }

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

            if (!AdvancedRecordFilters::Passes(entry, advancedRecordFilters)) {
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
                .selectedPluginDiagnostics = selectedPluginDiagnostics,
                .showPlayableRecords = showPlayableRecords,
                .showNonPlayableRecords = showNonPlayableRecords,
                .showNamedRecords = showNamedRecords,
                .showUnnamedRecords = showUnnamedRecords,
                .showDeletedRecords = showDeletedRecords,
                .advancedRules = advancedRecordFilters,
                .hiddenPlugins = hiddenPlugins,
                .advancedFilterRevision = advancedRecordFilterRevision,
                .showUnknownCategories = showUnknownCategories,
                .pluginGlobalSearchMode = pluginGlobalSearchMode,
                .showAdvancedDetailsView = Config::GetMutable().pluginAdvancedDetailsView,
                .equipWeaponAmmoCount = playerCurrentWeaponAmmoAmount,
                .searchFocusPending = &tabSearchFocusPending,
                .collapseSelectedRecordDiagnostics = collapseSelectedRecordDiagnostics,
                .favoriteForms = favoriteForms,
                .selectedPluginTreeRecordFormID = selectedPluginTreeRecordFormID,
                .selectedPluginTreeRecordFormIDs = selectedPluginTreeRecordFormIDs,
                .pluginTreeLastClickedFormID = pluginTreeLastClickedFormID,
                .recentPluginRecordFormIDs = recentPluginRecordFormIDs,
                .pluginBrowserCacheVersion = pluginBrowserCacheVersion,
                .pluginBrowserCacheSearch = pluginBrowserCacheSearch,
                .pluginBrowserCacheSelectedPlugin = pluginBrowserCacheSelectedPlugin,
                .pluginBrowserCacheShowPlayable = pluginBrowserCacheShowPlayable,
                .pluginBrowserCacheShowNonPlayable = pluginBrowserCacheShowNonPlayable,
                .pluginBrowserCacheShowNamed = pluginBrowserCacheShowNamed,
                .pluginBrowserCacheShowUnnamed = pluginBrowserCacheShowUnnamed,
                .pluginBrowserCacheShowDeleted = pluginBrowserCacheShowDeleted,
                .pluginBrowserCacheAdvancedFilterRevision = pluginBrowserCacheAdvancedFilterRevision,
                .pluginBrowserCacheShowUnknown = pluginBrowserCacheShowUnknown,
                .pluginBrowserCacheGlobalSearchMode = pluginBrowserCacheGlobalSearchMode,
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
                .openItemGrantPopupMultiple = [](const std::vector<FormEntry>& entries) {
                    OpenItemGrantPopupMultiple(entries);
                },
                .openGlobalValuePopup = [](std::uint32_t formID) {
                    MainWindowPopups::OpenGlobalValuePopup(formID);
                },
                .requestActionConfirmation = [](std::string title, std::string message, std::function<void()> callback) {
                    MainWindowPopups::RequestActionConfirmation(std::move(title), std::move(message), std::move(callback));
                },
                .passesAdvancedFilters = [](const FormEntry& entry) {
                    return AdvancedRecordFilters::Passes(entry, advancedRecordFilters);
                }
            };

            PluginBrowserTab::Draw(plugins, cache, dataVersion, context);
        }

        ContextMenuCallbacks BuildContextCallbacks()
        {
            ContextMenuCallbacks cb{};
            cb.localize = L;
            cb.openItemGrantPopup = [](const FormEntry& entry) {
                OpenItemGrantPopup(entry);
            };
            cb.openGlobalValuePopup = [](std::uint32_t formID) {
                MainWindowPopups::OpenGlobalValuePopup(formID);
            };
            cb.requestActionConfirmation = [](std::string title, std::string message, std::function<void()> callback) {
                MainWindowPopups::RequestActionConfirmation(std::move(title), std::move(message), std::move(callback));
            };
            cb.trackRecentRecord = [](std::uint32_t formID) {
                TrackRecentRecord(formID);
            };
            cb.favorites = &favoriteForms;
            cb.equipWeaponAmmoCount = playerCurrentWeaponAmmoAmount;
            return cb;
        }

        void DrawItemBrowser(const FormCache& cache)
        {
            auto itemContextCallbacks = BuildContextCallbacks();

            ItemBrowserTabContext context{
                .selectedPluginFilter = selectedPluginFilter,
                .itemSearch = itemSearch,
                .itemSearchBuffer = itemSearchBuffer,
                .itemSearchBufferSize = sizeof(itemSearchBuffer),
                .showPlayableRecords = showPlayableRecords,
                .showNonPlayableRecords = showNonPlayableRecords,
                .showNamedRecords = showNamedRecords,
                .showUnnamedRecords = showUnnamedRecords,
                .showDeletedRecords = showDeletedRecords,
                .advancedRules = advancedRecordFilters,
                .hiddenPlugins = hiddenPlugins,
                .advancedFilterRevision = advancedRecordFilterRevision,
                .searchFocusPending = &tabSearchFocusPending,
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
                .openItemGrantPopupMultiple = [](const std::vector<FormEntry>& entries) {
                    OpenItemGrantPopupMultiple(entries);
                },
                .tryGetEditorID = [](std::uint32_t formID) {
                    return TryGetEditorID(formID);
                },
                .passesAdvancedFilters = [](const FormEntry& entry) {
                    return AdvancedRecordFilters::Passes(entry, advancedRecordFilters);
                },
                .contextCallbacks = itemContextCallbacks
            };

            ItemBrowserTab::Draw(cache, context);
        }

        void DrawNPCBrowser(const FormCache& cache)
        {
            auto npcContextCallbacks = BuildContextCallbacks();
            NPCBrowserTab::Draw(
                cache,
                npcSearchBuffer,
                sizeof(npcSearchBuffer),
                npcSearch,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedRecordFilters,
                hiddenPlugins,
                advancedRecordFilterRevision,
                &tabSearchFocusPending,
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
                L,
                &npcContextCallbacks);
        }

        void DrawObjectBrowser(const FormCache& cache)
        {
            auto objectContextCallbacks = BuildContextCallbacks();
            ObjectBrowserTab::Draw(
                cache,
                objectSearchBuffer,
                sizeof(objectSearchBuffer),
                objectSearch,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedRecordFilters,
                hiddenPlugins,
                advancedRecordFilterRevision,
                &tabSearchFocusPending,
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
                L,
                &objectContextCallbacks);
        }

        void DrawCellBrowser(const FormCache& cache)
        {
            auto cellContextCallbacks = BuildContextCallbacks();
            CellBrowserTab::Draw(
                cache,
                cellSearchBuffer,
                sizeof(cellSearchBuffer),
                cellSearch,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedRecordFilters,
                hiddenPlugins,
                advancedRecordFilterRevision,
                &tabSearchFocusPending,
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
                L,
                &cellContextCallbacks);
        }

        void DrawSpellPerkBrowser(const FormCache& cache)
        {
            auto spellPerkContextCallbacks = BuildContextCallbacks();
            SpellPerkBrowserTab::Draw(
                cache,
                spellPerkSearchBuffer,
                sizeof(spellPerkSearchBuffer),
                spellPerkSearch,
                selectedPluginFilter,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedRecordFilters,
                hiddenPlugins,
                advancedRecordFilterRevision,
                &tabSearchFocusPending,
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
                L,
                &spellPerkContextCallbacks);
        }
    }

    void MainWindow::ResetStateFromConfig()
    {
        const auto& settings = Config::Get();

        selectedPluginFilter.clear();
        selectedPluginDiagnostics.clear();
        collapseSelectedRecordDiagnostics = false;
        pluginSearch.clear();
        itemSearch.clear();
        npcSearch.clear();
        objectSearch.clear();
        spellPerkSearch.clear();
        cellSearch.clear();

        pluginSearchBuffer[0] = '\0';
        itemSearchBuffer[0] = '\0';
        npcSearchBuffer[0] = '\0';
        objectSearchBuffer[0] = '\0';
        spellPerkSearchBuffer[0] = '\0';
        cellSearchBuffer[0] = '\0';
        InventoryTab::ResetState();

        favoriteForms.clear();
        favoriteForms.insert(settings.favorites.begin(), settings.favorites.end());

        selectedPluginTreeRecordFormID = 0;
        selectedPluginTreeRecordFormIDs.clear();
        pluginTreeLastClickedFormID = 0;
        selectedItemRows.clear();
        recentPluginRecordFormIDs.clear();

        showPlayableRecords = true;
        showNonPlayableRecords = settings.listShowNonPlayable;
        showNamedRecords = true;
        showUnnamedRecords = settings.listShowUnnamed;
        showDeletedRecords = settings.listShowDeleted;
        advancedRecordFilters = AdvancedRecordFilters::LoadRules(settings.advancedRecordFilters);
        hiddenPlugins = AdvancedRecordFilters::LoadHiddenPlugins(settings.hiddenPlugins);
        ++advancedRecordFilterRevision;
        showUnknownCategories = settings.pluginShowUnknownCategories;
        pluginGlobalSearchMode = settings.pluginGlobalSearchMode;

        pluginBrowserCacheVersion = 0;
        pluginBrowserCacheSearch.clear();
        pluginBrowserCacheSelectedPlugin.clear();
        pluginBrowserCacheAdvancedFilterRevision = 0;
        pluginBrowserGroupedRecordsCache.clear();
        pluginBrowserOrderedPluginsCache.clear();
        pluginBrowserGlobalSearchResultsCache.clear();

        const auto startupTab = ResolveStartupTab(settings);
        activeMainTab = (!settings.showLogsTab && startupTab == "Logs") ? "Plugin Browser" : startupTab;
        previousMainTab.clear();
        tabSearchFocusPending = settings.autoFocusSearchBars;
        collapseSelectedRecordDiagnostics = false;
    }

    void MainWindow::HandleMenuVisibilityChanged(bool visible)
    {
        MainWindowPopups::HandleMenuVisibilityChanged(visible);
        RecordFiltersWidget::HandleMenuVisibilityChanged(visible);
        Logger::Verbose(std::string("Main window visibility handler: ") + (visible ? "shown" : "hidden"));

        if (!visible) {
            ItemGrantPopup::Close();
            return;
        }

        if (Config::Get().autoFocusSearchBars) {
            tabSearchFocusPending = true;
        }
    }

    void MainWindow::Draw()
    {
        if (!DataManager::IsDataReady()) {
            DrawWaitingForDataHandlerPopup();
            return;
        }

        EnsureFavoritesLoaded();
        const auto favoritesBefore = favoriteForms;

        FormActions::ProcessPendingActions();
        const auto previousActiveTab = activeMainTab;

        if (refreshDataRequested && !refreshDataInProgress) {
            Logger::Info("Main window starting requested data refresh");
            refreshDataInProgress = true;
            DataManager::Refresh();
            FormTable::ClearCaches();
            PluginBrowserHelpers::ClearCaches();
            ContextMenu::ClearCaches();
            refreshDataInProgress = false;
            refreshDataRequested = false;
            Logger::Info("Main window finished requested data refresh");
        }

        const auto& settings = Config::Get();
        const float windowScale = (std::clamp)(settings.fontSize / 20.0f, 0.80f, 1.40f);
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 viewportPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
        const ImVec2 viewportSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
        const ImVec2 minWindowSize(
            (std::min)(1280.0f * windowScale, (std::max)(viewportSize.x, 1.0f)),
            (std::min)(720.0f * windowScale, (std::max)(viewportSize.y, 1.0f)));
        const ImVec2 maxSavedWindowSize(
            (std::max)(minWindowSize.x, viewportSize.x),
            (std::max)(minWindowSize.y, viewportSize.y));

        ImVec2 initialWindowPos(settings.windowX, settings.windowY);
        ImVec2 initialWindowSize(settings.windowW, settings.windowH);

        static bool initialPlacementChecked{ false };
        if (settings.rememberWindowPos && !initialPlacementChecked) {
            initialPlacementChecked = true;
            const ImVec2 clampedWindowSize = ClampWindowSizeToViewport(initialWindowSize, minWindowSize, maxSavedWindowSize);
            const ImVec2 clampedWindowPos = ClampWindowPosToViewport(initialWindowPos, clampedWindowSize, viewportPos, viewportSize);
            const bool adjustedSavedWindowPlacement =
                clampedWindowPos.x != initialWindowPos.x ||
                clampedWindowPos.y != initialWindowPos.y ||
                clampedWindowSize.x != initialWindowSize.x ||
                clampedWindowSize.y != initialWindowSize.y;

            if (adjustedSavedWindowPlacement) {
                auto& mutableSettings = Config::GetMutable();
                mutableSettings.windowX = clampedWindowPos.x;
                mutableSettings.windowY = clampedWindowPos.y;
                mutableSettings.windowW = clampedWindowSize.x;
                mutableSettings.windowH = clampedWindowSize.y;
                Config::RequestSave();
                Logger::Verbose("Adjusted saved main window placement to fit the active viewport");

                initialWindowPos = clampedWindowPos;
                initialWindowSize = clampedWindowSize;
            }
        }

        ImGui::SetNextWindowPos(initialWindowPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(initialWindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(minWindowSize, ImVec2(4096.0f, 4096.0f));

        const auto title = Language::Get("General", "sWindowTitle");
        const auto* windowTitle = title.empty() ? "ESP Explorer AE" : title.data();

        bool windowOpen = true;
        if (ImGui::Begin(windowTitle, &windowOpen)) {
            bool settingsDirty{ false };
            const ImVec2 menuWindowSize = ImGui::GetWindowSize();

            if (!settings.firstRunHelpDismissed) {
                MainWindowPopups::OpenFirstRunHelpOverlay();
            }

            if (!settings.showLogsTab && activeMainTab == "Logs") {
                activeMainTab = "Plugin Browser";
            }

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
                                    counts.activators + counts.containers + counts.statics + counts.furniture + counts.spells + counts.perks + counts.cells;
            const auto& tabLabels = GetMainTabLabels(cache, counts, dataVersion);

            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && FormActions::CanUndoLastAction()) {
                FormActions::UndoLastAction();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false) && !io.WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup)) {
                tabSearchFocusPending = true;
            }

            const auto& style = ImGui::GetStyle();
            const float footerTextRows = settings.showMenuResolutionInStatus ? 2.0f : 1.0f;
            const float rawFooterHeight = ImGui::GetTextLineHeightWithSpacing() * footerTextRows + ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y + style.WindowPadding.y + 10.0f;
            const float footerHeight = (std::min)(rawFooterHeight, (std::max)(0.0f, ImGui::GetContentRegionAvail().y - 1.0f));
            if (ImGui::BeginChild("MainContentRegion", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::BeginTabBar("MainTabs")) {
                    const auto visibleTabCount = GetVisibleTabCount(settings.showLogsTab);
                    if (!settings.showLogsTab && requestedMainTab == "Logs") {
                        requestedMainTab.clear();
                    }

                    if (Config::Get().enableGamepadNav && (GamepadInput::WasTabNextPressed() || GamepadInput::WasTabPrevPressed())) {
                        std::size_t currentIndex = 0;
                        for (std::size_t i = 0; i < visibleTabCount; ++i) {
                            if (activeMainTab == kMainTabOrder[i]) {
                                currentIndex = i;
                                break;
                            }
                        }

                        if (GamepadInput::WasTabNextPressed()) {
                            currentIndex = (currentIndex + 1) % visibleTabCount;
                        } else {
                            currentIndex = (currentIndex + visibleTabCount - 1) % visibleTabCount;
                        }
                        requestedMainTab = std::string(kMainTabOrder[currentIndex]);
                    }

                    auto tabFlags = [&](const char* tabName) -> ImGuiTabItemFlags {
                        return requestedMainTab == tabName ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                    };
                    auto focusTabIfRequested = [&](const char* tabName) {
                        if (requestedMainTab == tabName) {
                            ImGui::SetKeyboardFocusHere(-1);
                            requestedMainTab.clear();
                        }
                    };

                    const bool pluginTabOpen = ImGui::BeginTabItem(tabLabels.pluginLabel.c_str(), nullptr,
                        tabFlags("Plugin Browser"));
                    SharedUtils::DrawCurrentItemChrome(pluginTabOpen, ImGui::IsItemHovered(), true, false);
                    if (pluginTabOpen) {
                        activeMainTab = "Plugin Browser";
                        focusTabIfRequested("Plugin Browser");

                        if (refreshDataInProgress) {
                            ImGui::BeginDisabled(true);
                            ImGui::Button(L("General", "sRefreshData", "Refresh Data"));
                            ImGui::EndDisabled();
                        } else {
                            if (ImGui::Button(L("General", "sRefreshData", "Refresh Data"))) {
                                refreshDataRequested = true;
                                Logger::Verbose("Refresh Data requested from Plugin Browser tab");
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

                    const bool playerTabOpen = ImGui::BeginTabItem(tabLabels.inventoryLabel.c_str(), nullptr,
                        tabFlags("Inventory"));
                    SharedUtils::DrawCurrentItemChrome(playerTabOpen, ImGui::IsItemHovered(), true, false);
                    if (playerTabOpen) {
                        activeMainTab = "Inventory";
                        focusTabIfRequested("Inventory");
                        DrawInventoryTab(cache);
                        ImGui::EndTabItem();
                    }

                    const bool itemTabOpen = ImGui::BeginTabItem(tabLabels.itemLabel.c_str(), nullptr,
                        tabFlags("Item Browser"));
                    SharedUtils::DrawCurrentItemChrome(itemTabOpen, ImGui::IsItemHovered(), true, false);
                    if (itemTabOpen) {
                        activeMainTab = "Item Browser";
                        focusTabIfRequested("Item Browser");
                        DrawItemBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const bool npcTabOpen = ImGui::BeginTabItem(tabLabels.npcLabel.c_str(), nullptr,
                        tabFlags("NPC Browser"));
                    SharedUtils::DrawCurrentItemChrome(npcTabOpen, ImGui::IsItemHovered(), true, false);
                    if (npcTabOpen) {
                        activeMainTab = "NPC Browser";
                        focusTabIfRequested("NPC Browser");
                        DrawNPCBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const bool cellTabOpen = ImGui::BeginTabItem(tabLabels.cellLabel.c_str(), nullptr,
                        tabFlags("Cell Browser"));
                    SharedUtils::DrawCurrentItemChrome(cellTabOpen, ImGui::IsItemHovered(), true, false);
                    if (cellTabOpen) {
                        activeMainTab = "Cell Browser";
                        focusTabIfRequested("Cell Browser");
                        DrawCellBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const bool objectTabOpen = ImGui::BeginTabItem(tabLabels.objectLabel.c_str(), nullptr,
                        tabFlags("Object Browser"));
                    SharedUtils::DrawCurrentItemChrome(objectTabOpen, ImGui::IsItemHovered(), true, false);
                    if (objectTabOpen) {
                        activeMainTab = "Object Browser";
                        focusTabIfRequested("Object Browser");
                        DrawObjectBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const bool spellPerkTabOpen = ImGui::BeginTabItem(tabLabels.spellPerkLabel.c_str(), nullptr,
                        tabFlags("Spells & Perks"));
                    SharedUtils::DrawCurrentItemChrome(spellPerkTabOpen, ImGui::IsItemHovered(), true, false);
                    if (spellPerkTabOpen) {
                        activeMainTab = "Spells & Perks";
                        focusTabIfRequested("Spells & Perks");
                        DrawSpellPerkBrowser(cache);
                        ImGui::EndTabItem();
                    }

                    const bool settingsTabOpen = ImGui::BeginTabItem(tabLabels.settingsLabel.c_str(), nullptr,
                        tabFlags("Settings"));
                    SharedUtils::DrawCurrentItemChrome(settingsTabOpen, ImGui::IsItemHovered(), true, false);
                    if (settingsTabOpen) {
                        activeMainTab = "Settings";
                        focusTabIfRequested("Settings");
                        SettingsTab::Draw();
                        ImGui::EndTabItem();
                    }

                    if (settings.showLogsTab) {
                        const bool logsTabOpen = ImGui::BeginTabItem(tabLabels.logsLabel.c_str(), nullptr,
                            tabFlags("Logs"));
                        SharedUtils::DrawCurrentItemChrome(logsTabOpen, ImGui::IsItemHovered(), true, false);
                        if (logsTabOpen) {
                            activeMainTab = "Logs";
                            focusTabIfRequested("Logs");
                            LogViewerTab::Draw(L);
                            ImGui::EndTabItem();
                        }
                    }

                    ItemGrantPopup::Draw(L);
                    MainWindowPopups::Draw(L);

                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();

            if (ImGui::BeginChild("StatusBarRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                const float fps = io.Framerate;
                const float frameTime = fps > 0.0f ? (1000.0f / fps) : 0.0f;
                const float statusStartY = ImGui::GetCursorPosY();
                const float statusRowHeight = ImGui::GetTextLineHeightWithSpacing();

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
                    ImGui::TextDisabled("[%s]", L("Settings", "sGamepadStatus", "Gamepad"));
                }

                if (settings.showPlayerStatsInStatus) {
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    auto* av = RE::ActorValue::GetSingleton();
                    auto* ui = RE::UI::GetSingleton();
                    const bool inMainMenu = ui && ui->GetMenuOpen<RE::MainMenu>();

                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();

                    if (player && av && av->health && av->actionPoints && !inMainMenu) {
                        const auto level = player->GetLevel();
                        const auto caps = player->GetGoldAmount();
                        const float hp = player->GetActorValue(*av->health);
                        const float ap = player->GetActorValue(*av->actionPoints);
                        ImGui::TextDisabled("%s %d  %s %lld  %s %.0f  %s %.0f", L("General", "sLevelShort", ""), level, L("Inventory", "sCaps", ""), caps, L("Inventory", "sHealthShort", ""), hp, L("Inventory", "sActionPointsShort", ""), ap);
                    } else {
                        ImGui::TextDisabled("%s --  %s --  %s --  %s --", L("General", "sLevelShort", ""), L("Inventory", "sCaps", ""), L("Inventory", "sHealthShort", ""), L("Inventory", "sActionPointsShort", ""));
                    }
                }

                const float resetWidth = CalcButtonWidth(L("General", "sResetFilters", "Reset Filters"));
                const float historyWidth = CalcButtonWidth(L("General", "sActionHistory", "Action History"));
                const float undoWidth = CalcButtonWidth(L("General", "sUndoLastAction", "Undo Last Action"));
                const float actionsWidth = resetWidth + style.ItemSpacing.x + historyWidth + style.ItemSpacing.x + undoWidth;
                const float actionStartX = ImGui::GetWindowContentRegionMax().x - actionsWidth;
                ImGui::SetCursorPos(ImVec2((std::max)(actionStartX, ImGui::GetCursorPosX()), statusStartY));

                if (ImGui::Button(L("General", "sResetFilters", "Reset Filters"))) {
                    ResetQuickFilters();
                }
                ImGui::SameLine();
                if (ImGui::Button(L("General", "sActionHistory", "Action History"))) {
                    ImGui::OpenPopup("##ActionHistoryPopup");
                }
                DrawActionHistoryPopup();
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

                if (settings.showMenuResolutionInStatus) {
                    ImGui::SetCursorPosY(statusStartY + statusRowHeight + 2.0f);
                    ImGui::TextDisabled("%s: %dx%d", L("Settings", "sResolutionShort", "Res"), static_cast<int>(menuWindowSize.x), static_cast<int>(menuWindowSize.y));
                }
            }
            ImGui::EndChild();

            if (favoriteForms != favoritesBefore) {
                PersistFavoriteForms();
            }

            if (activeMainTab != previousActiveTab && Config::Get().autoFocusSearchBars) {
                tabSearchFocusPending = true;
            }

            if (activeMainTab != previousActiveTab && !activeMainTab.empty()) {
                Logger::Verbose("Active tab changed to: " + activeMainTab);
            }

            auto& mutableSettings = Config::GetMutable();
            if (mutableSettings.lastActiveTab != activeMainTab && !activeMainTab.empty()) {
                mutableSettings.lastActiveTab = activeMainTab;
                settingsDirty = true;
            }

            if (settingsDirty) {
                Config::RequestSave();
            }
        }
        ImGui::End();
        if (!windowOpen) {
            Logger::Verbose("Main window close button pressed");
            Hooks::SetMenuVisible(false);
        }
    }
}
