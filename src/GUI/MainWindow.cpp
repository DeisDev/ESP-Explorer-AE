#include "GUI/MainWindow.h"

#include "Core/CatalogCounts.h"
#include "Core/RecordActions.h"
#include "Core/Profiling.h"
#include "Core/ScopeExit.h"

#include "Config/Config.h"
#include "App/ActionService.h"
#include "App/FavoriteService.h"
#include "App/SettingsService.h"
#include "App/DetailService.h"
#include "App/InventoryService.h"
#include "App/PlayerStatusService.h"
#include "App/CatalogService.h"
#include "Filters/AdvancedRecordFilters.h"
#include "Core/CatalogQuery.h"
#include "GUI/Tabs/ItemBrowserTab.h"
#include "GUI/Tabs/InventoryTab.h"
#include "GUI/Tabs/LogViewerTab.h"
#include "App/LogService.h"
#include "GUI/Tabs/NPCBrowserTab.h"
#include "GUI/Tabs/CellBrowserTab.h"
#include "GUI/Tabs/ObjectBrowserTab.h"
#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Tabs/SettingsTab.h"
#include "GUI/Tabs/SpellPerkBrowserTab.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ActionHistory.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/ItemGrantPopup.h"
#include "GUI/Widgets/MainWindowPopups.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"
#include "Hooks/Hooks.h"
#include "Input/GamepadInput.h"
#include "Localization/Language.h"

#include <imgui.h>
#include <imgui_internal.h>



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

        std::string selectedPluginFilter{};


        SettingsTabState settingsTab;
        LogViewerState logViewer;
        ItemGrantPopup itemGrantPopup;
        MainWindowPopups mainWindowPopups;
        bool browserSettingsInitialized{ false };
        std::string activeMainTab{};
        std::string previousMainTab{};
        std::string requestedMainTab{};
        bool tabSearchFocusPending{ false };

        PluginBrowserState pluginBrowser;
        InventoryTabState inventoryBrowser;
        std::optional<DetailKey> inventoryDetailRequest;
        SharedBrowserFilters browserFilters;
        BrowserState itemBrowser;
        NPCBrowserState npcBrowser;
        BrowserState cellBrowser;
        BrowserState objectBrowser;
        BrowserState spellPerkBrowser;
        bool discardPopupStack{};
        bool refreshDataRequested{ false };
        bool refreshDataInProgress{ false };

        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::FrameText(section, key);
            return value.empty() ? fallback : value.data();
        }

        void ResetQuickFilters()
        {
            selectedPluginFilter.clear();
            pluginBrowser.diagnosticsPlugin.clear();
            pluginBrowser.search.clear();
            itemBrowser.search.clear();
            npcBrowser.browser.search.clear();
            objectBrowser.search.clear();
            spellPerkBrowser.search.clear();
            cellBrowser.search.clear();

            pluginBrowser.searchBuffer[0] = '\0';
            itemBrowser.searchBuffer[0] = '\0';
            npcBrowser.browser.searchBuffer[0] = '\0';
            objectBrowser.searchBuffer[0] = '\0';
            spellPerkBrowser.searchBuffer[0] = '\0';
            cellBrowser.searchBuffer[0] = '\0';
        }

        void DrawPluginFilterStatus()
        {
            ImGui::Text("%s: %s", L("PluginBrowser", "sFilter", "Filter"), selectedPluginFilter.empty() ? L("General", "sNone", "None") : selectedPluginFilter.c_str());
        }

        void TrackRecentRecord(std::uint32_t formID)
        {
            pluginBrowser.TrackRecent(formID, static_cast<std::size_t>((std::clamp)(Config::Get().recentRecordsLimit, 5, 100)));
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

            const auto receipts = ActionService::Receipts();
            const auto history = FormatActionHistory(receipts, L);
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
                    ImGui::TextDisabled("%s", entry.status.c_str());
                    if (!entry.details.empty() && ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", entry.details.c_str());
                    }

                    const float buttonX = ImGui::GetWindowContentRegionMax().x - undoButtonWidth;
                    ImGui::SetCursorPos(ImVec2(buttonX, style.WindowPadding.y));
                    // No inverse is offered without a verified restoration
                    // contract. Preserve the existing disabled history control.
                    ImGui::BeginDisabled(true);
                    ImGui::Button(L("General", "sNoUndoAvailable", "No Undo"), ImVec2(undoButtonWidth, 0.0f));
                    ImGui::EndDisabled();

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

        const PreparedRecordFilters& PreparedFilters()
        {
            static PreparedRecordFilters prepared;
            static std::uint64_t revision{};
            if (revision != browserFilters.advancedRecordFilterRevision) {
                prepared = PreparedRecordFilters(browserFilters.advancedRecordFilters);
                revision = browserFilters.advancedRecordFilterRevision;
            }
            return prepared;
        }


        struct MainTabLabelsCache
        {
            std::uint64_t dataVersion{ (std::numeric_limits<std::uint64_t>::max)() };
            std::string languageCode;
            CatalogQuery query;
            std::uint64_t filterRevision{};
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

        void FormatTabLabel(char* buf, std::size_t bufSize, const char* label, std::size_t filtered, std::size_t total, const char* stableId)
        {
            if (filtered < total) {
                std::snprintf(buf, bufSize, "%s (%zu / %zu)###%s", label, filtered, total, stableId);
            } else {
                std::snprintf(buf, bufSize, "%s (%zu)###%s", label, total, stableId);
            }
        }

        const MainTabLabelsCache& GetMainTabLabels(const CatalogSnapshot& catalog)
        {
            static MainTabLabelsCache cache;
            const auto language = Language::GetCurrentLanguageCode();
            CatalogQuery query;
            query.showPlayable = browserFilters.showPlayableRecords;
            query.showNonPlayable = browserFilters.showNonPlayableRecords;
            query.showNamed = browserFilters.showNamedRecords;
            query.showUnnamed = browserFilters.showUnnamedRecords;
            query.showDeleted = browserFilters.showDeletedRecords;
            query.hiddenPlugins = browserFilters.hiddenPlugins;
            if (cache.dataVersion == catalog.generation && cache.languageCode == language && cache.query == query &&
                cache.filterRevision == browserFilters.advancedRecordFilterRevision) return cache;
            cache.dataVersion = catalog.generation;
            cache.languageCode = language;
            cache.query = query;
            cache.filterRevision = browserFilters.advancedRecordFilterRevision;
            const auto counts = CountCatalog(catalog, query, PreparedFilters());
            const auto label = [&](const char* text, RecordCount count, const char* id) {
                char buffer[120]{};
                FormatTabLabel(buffer, sizeof(buffer), text, count.filtered, count.total, id);
                return std::string(buffer);
            };
            cache.pluginLabel = L("PluginBrowser", "sBrowserTab", "Plugin Browser");
            cache.inventoryLabel = L("Inventory", "sTabName", "Inventory");
            cache.itemLabel = label(L("Items", "sBrowserTab", "Item Browser"), counts.For({"WEAP", "ARMO", "AMMO", "MISC", "KEYM", "NOTE", "BOOK", "ALCH", "CMPO"}), "MainTabItem");
            cache.npcLabel = label(L("NPCs", "sBrowserTab", "NPC Browser"), counts.For({"NPC_"}), "MainTabNPC");
            cache.cellLabel = label(L("Cells", "sBrowserTab", "Cell Browser"), counts.For({"CELL"}), "MainTabCell");
            cache.objectLabel = label(L("Objects", "sBrowserTab", "Object Browser"), counts.For({"ACTI", "CONT", "STAT", "FURN"}), "MainTabObject");
            cache.spellPerkLabel = label(L("Spells", "sBrowserTab", "Spells & Perks"), counts.For({"SPEL", "PERK"}), "MainTabSpells");
            cache.settingsLabel = L("Settings", "sTabName", "Settings");
            cache.logsLabel = L("Logs", "sTabName", "Logs");
            return cache;
        }

        std::size_t GetVisibleTabCount(bool showLogsTab)
        {
            return showLogsTab ? kMainTabOrder.size() : (kMainTabOrder.size() - 1);
        }


        void PersistListFilterSettings(bool revise = true)
        {
            if (revise) ++browserFilters.advancedRecordFilterRevision;
            auto& settings = Config::GetMutable();
            settings.listShowNonPlayable = browserFilters.showNonPlayableRecords;
            settings.listShowUnnamed = browserFilters.showUnnamedRecords;
            settings.listShowDeleted = browserFilters.showDeletedRecords;
            settings.advancedRecordFilters = AdvancedRecordFilters::SaveRules(browserFilters.advancedRecordFilters);
            settings.hiddenPlugins = AdvancedRecordFilters::SaveHiddenPlugins(browserFilters.hiddenPlugins);
            Config::RequestSave();
        }

        void PersistFilterCheckboxSettings()
        {
            auto& settings = Config::GetMutable();
            settings.listShowNonPlayable = browserFilters.showNonPlayableRecords;
            settings.listShowUnnamed = browserFilters.showUnnamedRecords;
            settings.listShowDeleted = browserFilters.showDeletedRecords;
            settings.advancedRecordFilters = AdvancedRecordFilters::SaveRules(browserFilters.advancedRecordFilters);
            settings.hiddenPlugins = AdvancedRecordFilters::SaveHiddenPlugins(browserFilters.hiddenPlugins);
            settings.pluginGlobalSearchMode = pluginBrowser.globalSearch;
            settings.pluginShowUnknownCategories = pluginBrowser.showUnknown;
            Config::RequestSave();
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

        void EnsureBrowserSettingsLoaded()
        {
            if (browserSettingsInitialized) {
                return;
            }

            const auto& settings = Config::Get();
            activeMainTab = ResolveStartupTab(settings);
            if (!settings.showLogsTab && activeMainTab == "Logs") {
                activeMainTab = "Plugin Browser";
            }
            browserFilters.showPlayableRecords = true;
            browserFilters.showNonPlayableRecords = settings.listShowNonPlayable;
            browserFilters.showNamedRecords = true;
            browserFilters.showUnnamedRecords = settings.listShowUnnamed;
            browserFilters.showDeletedRecords = settings.listShowDeleted;
            browserFilters.advancedRecordFilters = AdvancedRecordFilters::LoadRules(settings.advancedRecordFilters);
            browserFilters.hiddenPlugins = AdvancedRecordFilters::LoadHiddenPlugins(settings.hiddenPlugins);
            ++browserFilters.advancedRecordFilterRevision;
            pluginBrowser.globalSearch = settings.pluginGlobalSearchMode;
            pluginBrowser.showUnknown = settings.pluginShowUnknownCategories;
            browserSettingsInitialized = true;
        }

        void OpenItemGrantPopup(const FormEntry& entry)
        {
            if (!ActionService::IsReady()) {
                return;
            }

            itemGrantPopup.Open(entry, ActionService::Session());
        }


        void HandleBrowserRequests(BrowserRequests& requests, ActionAdmission& admission)
        {
            for (const auto& grant : requests.grants) {
                if (grant.session != ActionService::Session()) { admission = ActionAdmission::StaleSession; continue; }
                if (!ActionService::IsReady()) { admission = ActionAdmission::Unavailable; continue; }
                const auto selected = SelectGrantRecords(CatalogService::Read(), grant.forms);
                if (!selected) { admission = ActionAdmission::Invalid; continue; }
                std::vector<FormEntry> entries;
                for (std::size_t row = 0; row < selected->order.size(); ++row) entries.push_back(selected->At(row));
                admission = ActionAdmission::Accepted;
                itemGrantPopup.Open(entries, grant.session);
            }
            for (const auto& global : requests.globalValues) {
                if (global.session != ActionService::Session()) { admission = ActionAdmission::StaleSession; continue; }
                if (!ActionService::IsReady()) { admission = ActionAdmission::Unavailable; continue; }
                const auto catalog = CatalogService::Read();
                const auto* record = catalog->Find(global.formID);
                if (!record || !PrepareRecordAction(ActionKind::SetGlobal, *record, global.session, true)) { admission = ActionAdmission::Invalid; continue; }
                mainWindowPopups.OpenGlobalValuePopup(global.formID, record->editorID, global.session);
            }
            for (auto id : requests.recentSelections) TrackRecentRecord(id);
            if (requests.filtersChanged) PersistListFilterSettings(false);
            std::vector<ActionRequest> immediate;
            for (auto& action : requests.actions) {
                if (action.confirm) {
                    std::string title = L("General", "sConfirmAction", "Confirm Action");
                    std::string body;
                    switch (action.request.kind) {
                    case ActionKind::Teleport:
                        title = L("General", "sConfirmTeleportTitle", "Confirm Teleport");
                        body = L("General", "sConfirmTeleport", "Teleport to selected destination?");
                        break;
                    case ActionKind::Spawn: case ActionKind::Place:
                        title = L("General", "sConfirmSpawnTitle", "Confirm Spawn");
                        body = std::string(L("General", "sConfirmSpawnMessage", "Spawn selected record at player?")) + "\n" + (action.targetName.empty() ? L("General", "sUnnamed", "<Unnamed>") : action.targetName);
                        break;
                    case ActionKind::AddSpell: body = L("General", "sConfirmAddSpellEffect", "Add selected spell/effect to player?"); break;
                    case ActionKind::RemoveSpell: body = L("General", "sConfirmRemoveSpellEffect", "Remove selected spell/effect from player?"); break;
                    case ActionKind::AddPerk: body = L("General", "sConfirmAddPerk", "Add selected perk to player?"); break;
                    case ActionKind::RemovePerk: body = L("General", "sConfirmRemovePerk", "Remove selected perk from player?"); break;
                    case ActionKind::StartQuest:
                        title = L("General", "sConfirmQuestTitle", "Confirm Quest Action");
                        body = L("General", "sConfirmStartQuest", "Start selected quest?");
                        break;
                    case ActionKind::CompleteQuest:
                        title = L("General", "sConfirmQuestTitle", "Confirm Quest Action");
                        body = L("General", "sConfirmCompleteQuest", "Complete selected quest?");
                        break;
                    case ActionKind::SetWeather:
                        title = L("General", "sConfirmWeatherTitle", "Confirm Weather Change");
                        body = L("General", "sConfirmWeather", "Set current weather to selected weather record?");
                        break;
                    case ActionKind::Outfit: body = L("General", "sConfirmAddOutfitItems", "Add all items from selected outfit to player?"); break;
                    case ActionKind::ConstructedItem: body = L("General", "sConfirmAddCraftedItem", "Add crafted output of selected recipe to player?"); break;
                    default: break;
                    }
                    mainWindowPopups.RequestActionConfirmation(std::move(title), std::move(body),
                        { std::move(action.request) });
                } else immediate.push_back(std::move(action.request));
            }
            if (!immediate.empty()) admission = ActionService::SubmitBatch(std::move(immediate));
        }

        void DrawInventoryTab(std::shared_ptr<const CatalogSnapshot> catalog)
        {
            const auto session = ActionService::Session();
            const bool advanced = Config::Get().pluginAdvancedDetailsView;
            InventoryTabView view{ .localize = L, .catalog = std::move(catalog), .inventory = InventoryService::Request(session),
                .session = session, .gameplayReady = ActionService::IsReady(), .godMode = ActionService::GodModeEnabled(), .advancedDetails = advanced };
            if (inventoryDetailRequest && inventoryDetailRequest->session == session && inventoryDetailRequest->catalogGeneration == view.catalog->generation && inventoryDetailRequest->advanced == advanced)
                view.details = DetailService::Request(*inventoryDetailRequest);
            if (tabSearchFocusPending) {
                inventoryBrowser.focusPending = true;
                tabSearchFocusPending = false;
            }
            InventoryTabRequests requests;
            InventoryTab::Draw(inventoryBrowser, view, requests);
            HandleBrowserRequests(requests.records, inventoryBrowser.admission);
            for (auto& confirmation : requests.confirmations) {
                mainWindowPopups.RequestActionConfirmation(std::move(confirmation.title), std::move(confirmation.message),
                    std::move(confirmation.actions), MainWindowPopups::Origin::Inventory);
            }
            if (requests.refresh) InventoryService::Request(session, true);
            inventoryDetailRequest = requests.details;
            if (requests.details) DetailService::Request(*requests.details);
            if (requests.inspect) {
                const auto formID = *requests.inspect;
                const auto formIDText = FormatUtils::FormID(formID);
                requestedMainTab = "Plugin Browser";
                selectedPluginFilter.clear();
                pluginBrowser.diagnosticsPlugin.clear();
                pluginBrowser.globalSearch = true;
                pluginBrowser.search = formIDText;
                std::snprintf(pluginBrowser.searchBuffer.data(), pluginBrowser.searchBuffer.size(), "%s", formIDText.c_str());
                pluginBrowser.selection.Single({ "GlobalResult", formID });
                TrackRecentRecord(formID);
            }
        }

        void DrawPluginBrowser(std::shared_ptr<const CatalogSnapshot> snapshot)
        {
            if (tabSearchFocusPending) {
                pluginBrowser.focusPending = true;
                tabSearchFocusPending = false;
            }
            const auto& config = Config::Get();
            PluginBrowserView view{
                .records = { snapshot, browserFilters, FavoriteService::Forms(), selectedPluginFilter,
                    ActionService::Session(), ActionService::IsReady(), L, {}, config.autoFocusSearchBars,
                    static_cast<std::uint32_t>((std::max)(0, inventoryBrowser.quick.currentAmmo)) },
                .advancedDetails = config.pluginAdvancedDetailsView,
                .details = {},
                .recentLimit = static_cast<std::size_t>((std::clamp)(config.recentRecordsLimit, 5, 100)),
                .copyFormat = config.multiCopyFormat,
                .favoriteReviewCount = FavoriteService::Review().legacy.size() + FavoriteService::Review().unresolved.size()
            };
            if (const auto id = pluginBrowser.selection.records.active; id && snapshot->Find(id)) {
                view.details = DetailService::Request({ id, view.records.session, snapshot->generation, view.advancedDetails });
            }
            PluginBrowserRequests requests;
            PluginBrowserTab::Draw(pluginBrowser, view, requests);
            HandleBrowserRequests(requests.records, pluginBrowser.admission);
            if (requests.pluginFilter) selectedPluginFilter = std::move(*requests.pluginFilter);
            if (requests.settingsChanged) PersistFilterCheckboxSettings();
            if (requests.details) DetailService::Request(*requests.details);
        }

        template <class Draw>
        void DrawCatalogBrowser(BrowserState& browser, std::shared_ptr<const CatalogSnapshot> catalog, Draw&& draw)
        {
            if (tabSearchFocusPending) {
                browser.focusPending = true;
                tabSearchFocusPending = false;
            }
            BrowserRequests requests;
            draw(browser, { std::move(catalog), browserFilters, FavoriteService::Forms(), selectedPluginFilter,
                ActionService::Session(), ActionService::IsReady(), L, DrawPluginFilterStatus, Config::Get().autoFocusSearchBars,
                static_cast<std::uint32_t>((std::max)(0, inventoryBrowser.quick.currentAmmo)), Config::Get().multiCopyFormat }, requests);
            HandleBrowserRequests(requests, browser.admission);
        }


    }

    void MainWindow::ResetStateFromConfig()
    {
        const auto& settings = Config::Get();
        itemGrantPopup.Close();
        mainWindowPopups.HandleMenuVisibilityChanged(false);
        discardPopupStack = true;

        selectedPluginFilter.clear();
        pluginBrowser.diagnosticsPlugin.clear();
        pluginBrowser.collapseDiagnostics = false;
        pluginBrowser.search.clear();
        pluginBrowser.searchBuffer.fill(0);
        itemBrowser.search.clear();
        npcBrowser.browser.search.clear();
        objectBrowser.search.clear();
        spellPerkBrowser.search.clear();
        cellBrowser.search.clear();

        itemBrowser.searchBuffer[0] = '\0';
        npcBrowser.browser.searchBuffer[0] = '\0';
        objectBrowser.searchBuffer[0] = '\0';
        spellPerkBrowser.searchBuffer[0] = '\0';
        cellBrowser.searchBuffer[0] = '\0';
        InventoryTab::ResetState(inventoryBrowser);
        inventoryDetailRequest.reset();

        FavoriteService::Reset();
        settingsTab = {};
        logViewer = {};
        LogService::Reset();
        itemBrowser.ResetSession();
        npcBrowser.ResetSession();
        objectBrowser.ResetSession();
        spellPerkBrowser.ResetSession();
        cellBrowser.ResetSession();
        pluginBrowser.ResetSession();

        browserFilters.showPlayableRecords = true;
        browserFilters.showNonPlayableRecords = settings.listShowNonPlayable;
        browserFilters.showNamedRecords = true;
        browserFilters.showUnnamedRecords = settings.listShowUnnamed;
        browserFilters.showDeletedRecords = settings.listShowDeleted;
        browserFilters.advancedRecordFilters = AdvancedRecordFilters::LoadRules(settings.advancedRecordFilters);
        browserFilters.hiddenPlugins = AdvancedRecordFilters::LoadHiddenPlugins(settings.hiddenPlugins);
        ++browserFilters.advancedRecordFilterRevision;
        pluginBrowser.showUnknown = settings.pluginShowUnknownCategories;
        pluginBrowser.globalSearch = settings.pluginGlobalSearchMode;


        const auto startupTab = ResolveStartupTab(settings);
        activeMainTab = (!settings.showLogsTab && startupTab == "Logs") ? "Plugin Browser" : startupTab;
        previousMainTab.clear();
        tabSearchFocusPending = settings.autoFocusSearchBars;
        pluginBrowser.collapseDiagnostics = false;
    }

    void MainWindow::Shutdown()
    {
        // Render-owner cleanup only; no config reset or gameplay effects.
        itemGrantPopup.Close();
        mainWindowPopups.HandleMenuVisibilityChanged(false);
        settingsTab = {};
        logViewer = {};
        LogService::Reset();
        pluginBrowser = {};
        inventoryBrowser = {};
        inventoryDetailRequest.reset();
        itemBrowser = {};
        npcBrowser = {};
        cellBrowser = {};
        objectBrowser = {};
        spellPerkBrowser = {};
        selectedPluginFilter.clear();
        FavoriteService::Reset();
        PerformanceProfile().Observe(ProfileGauge::QueryIndexBytes, 0);
        discardPopupStack = true;
    }

    void MainWindow::HandleMenuVisibilityChanged(bool visible)
    {
        const ProfileScope profileScope(visible ? ProfileMetric::MenuOpen : ProfileMetric::MenuClose);
        mainWindowPopups.HandleMenuVisibilityChanged(visible);
        for (auto* editor : { &pluginBrowser.filterEditor, &itemBrowser.filterEditor, &npcBrowser.browser.filterEditor,
                 &objectBrowser.filterEditor, &spellPerkBrowser.filterEditor, &cellBrowser.filterEditor }) editor->HandleMenuVisibilityChanged(visible);
        REX::DEBUG("{}", std::string("Main window visibility handler: ") + (visible ? "shown" : "hidden"));

        if (!visible) {
            discardPopupStack = true;
            itemGrantPopup.Close();
            settingsTab.Close();
            return;
        }

        if (Config::Get().autoFocusSearchBars) {
            tabSearchFocusPending = true;
        }
    }

    void MainWindow::Draw()
    {
        const ScopeExit observeQueryStorage([] {
            if (!PerformanceProfile().Enabled()) return;
            auto bytes = pluginBrowser.query.IndexBytes() + pluginBrowser.filterEditor.pluginOrder.capacity() * sizeof(std::size_t);
            for (const auto* browser : { &itemBrowser, &npcBrowser.browser, &objectBrowser, &spellPerkBrowser, &cellBrowser }) {
                bytes += browser->filterEditor.pluginOrder.capacity() * sizeof(std::size_t);
                for (const auto& [category, rows] : browser->categories)
                    bytes += rows.query.IndexBytes() + rows.table.displayedIDs.capacity() * sizeof(std::uint32_t);
            }
            PerformanceProfile().Observe(ProfileGauge::QueryIndexBytes, bytes);
        });
        static std::uint64_t lastSession{};
        const auto session = ActionService::Session();
        if (lastSession != session) {
            lastSession = session;
            discardPopupStack = true;
            mainWindowPopups.HandleMenuVisibilityChanged(false);
            itemGrantPopup.Close();
            settingsTab.Close();
            InventoryTab::ResetState(inventoryBrowser);
            inventoryDetailRequest.reset();
            cellBrowser.ResetSession();
            itemBrowser.ResetSession();
            npcBrowser.ResetSession();
            objectBrowser.ResetSession();
            spellPerkBrowser.ResetSession();
            pluginBrowser.ResetSession();
        }
        if (discardPopupStack) {
            ImGui::ClosePopupsOverWindow(nullptr, false);
            discardPopupStack = false;
        }
        if (!CatalogService::Read()->ready) {
            DrawWaitingForDataHandlerPopup();
            return;
        }

        EnsureBrowserSettingsLoaded();

        const auto previousActiveTab = activeMainTab;

        if (refreshDataRequested && !refreshDataInProgress) {
            REX::INFO("{}", "Main window starting requested data refresh");
            refreshDataInProgress = true;
            CatalogService::RequestRefresh();
            refreshDataInProgress = false;
            refreshDataRequested = false;
            REX::INFO("{}", "Main window finished requested data refresh");
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
                REX::DEBUG("{}", "Adjusted saved main window placement to fit the active viewport");

                initialWindowPos = clampedWindowPos;
                initialWindowSize = clampedWindowSize;
            }
        }

        ImGui::SetNextWindowPos(initialWindowPos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(initialWindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(minWindowSize, ImVec2(4096.0f, 4096.0f));

        const auto title = Language::FrameText("General", "sWindowTitle");
        const auto* windowTitle = title.empty() ? "ESP Explorer AE" : title.data();

        bool windowOpen = true;
        if (ImGui::Begin(windowTitle, &windowOpen)) {
            bool settingsDirty{ false };
            const ImVec2 menuWindowSize = ImGui::GetWindowSize();

            if (!settings.firstRunHelpDismissed) {
                mainWindowPopups.OpenFirstRunHelpOverlay(settings.firstRunHelpDismissed);
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

            const auto catalog = CatalogService::Read();
            const auto& plugins = catalog->plugins;
            FavoriteService::Prepare(catalog, session);
            const auto favoritesBefore = FavoriteService::Forms();
            const auto totalForms = catalog->records.size();
            const auto& tabLabels = GetMainTabLabels(*catalog);

            ImGuiIO& io = ImGui::GetIO();
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
                                REX::DEBUG("{}", "Refresh Data requested from Plugin Browser tab");
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

                        DrawPluginBrowser(catalog);
                        ImGui::EndTabItem();
                    }

                    const bool playerTabOpen = ImGui::BeginTabItem(tabLabels.inventoryLabel.c_str(), nullptr,
                        tabFlags("Inventory"));
                    SharedUtils::DrawCurrentItemChrome(playerTabOpen, ImGui::IsItemHovered(), true, false);
                    if (playerTabOpen) {
                        activeMainTab = "Inventory";
                        focusTabIfRequested("Inventory");
                        DrawInventoryTab(catalog);
                        ImGui::EndTabItem();
                    }

                    const bool itemTabOpen = ImGui::BeginTabItem(tabLabels.itemLabel.c_str(), nullptr,
                        tabFlags("Item Browser"));
                    SharedUtils::DrawCurrentItemChrome(itemTabOpen, ImGui::IsItemHovered(), true, false);
                    if (itemTabOpen) {
                        activeMainTab = "Item Browser";
                        focusTabIfRequested("Item Browser");
                        DrawCatalogBrowser(itemBrowser, catalog, ItemBrowserTab::Draw);
                        ImGui::EndTabItem();
                    }

                    const bool npcTabOpen = ImGui::BeginTabItem(tabLabels.npcLabel.c_str(), nullptr,
                        tabFlags("NPC Browser"));
                    SharedUtils::DrawCurrentItemChrome(npcTabOpen, ImGui::IsItemHovered(), true, false);
                    if (npcTabOpen) {
                        activeMainTab = "NPC Browser";
                        focusTabIfRequested("NPC Browser");
                        DrawCatalogBrowser(npcBrowser.browser, catalog, [](BrowserState&, const BrowserView& view, BrowserRequests& requests) {
                            NPCBrowserTab::Draw(npcBrowser, view, requests);
                        });
                        ImGui::EndTabItem();
                    }

                    const bool cellTabOpen = ImGui::BeginTabItem(tabLabels.cellLabel.c_str(), nullptr,
                        tabFlags("Cell Browser"));
                    SharedUtils::DrawCurrentItemChrome(cellTabOpen, ImGui::IsItemHovered(), true, false);
                    if (cellTabOpen) {
                        activeMainTab = "Cell Browser";
                        focusTabIfRequested("Cell Browser");
                        DrawCatalogBrowser(cellBrowser, catalog, CellBrowserTab::Draw);
                        ImGui::EndTabItem();
                    }

                    const bool objectTabOpen = ImGui::BeginTabItem(tabLabels.objectLabel.c_str(), nullptr,
                        tabFlags("Object Browser"));
                    SharedUtils::DrawCurrentItemChrome(objectTabOpen, ImGui::IsItemHovered(), true, false);
                    if (objectTabOpen) {
                        activeMainTab = "Object Browser";
                        focusTabIfRequested("Object Browser");
                        DrawCatalogBrowser(objectBrowser, catalog, ObjectBrowserTab::Draw);
                        ImGui::EndTabItem();
                    }

                    const bool spellPerkTabOpen = ImGui::BeginTabItem(tabLabels.spellPerkLabel.c_str(), nullptr,
                        tabFlags("Spells & Perks"));
                    SharedUtils::DrawCurrentItemChrome(spellPerkTabOpen, ImGui::IsItemHovered(), true, false);
                    if (spellPerkTabOpen) {
                        activeMainTab = "Spells & Perks";
                        focusTabIfRequested("Spells & Perks");
                        DrawCatalogBrowser(spellPerkBrowser, catalog, SpellPerkBrowserTab::Draw);
                        ImGui::EndTabItem();
                    }

                    const bool settingsTabOpen = ImGui::BeginTabItem(tabLabels.settingsLabel.c_str(), nullptr,
                        tabFlags("Settings"));
                    SharedUtils::DrawCurrentItemChrome(settingsTabOpen, ImGui::IsItemHovered(), true, false);
                    if (settingsTabOpen) {
                        activeMainTab = "Settings";
                        focusTabIfRequested("Settings");
                        SettingsTabRequests requests;
                        const auto favoriteReview = FavoriteService::Review();
                        const auto resources = SettingsService::Read();
                        SettingsTab::Draw(settingsTab, { Config::Get(), resources, favoriteReview, L }, requests);
                        SettingsService::Apply(std::move(requests.settings), requests.reloadThemes);
                        if (requests.resetAll) MainWindow::ResetStateFromConfig();
                        if (!requests.resetAll && !requests.favorites.accepted.empty() && !FavoriteService::AcceptLegacy(requests.favorites.review, requests.favorites.accepted)) settingsTab.favorites.stale = true;
                        if (requests.page) SettingsService::OpenPage(*requests.page);
                        if (requests.showHelp) mainWindowPopups.OpenHelpOverlay();
                        ImGui::EndTabItem();
                    }

                    if (settings.showLogsTab) {
                        const bool logsTabOpen = ImGui::BeginTabItem(tabLabels.logsLabel.c_str(), nullptr,
                            tabFlags("Logs"));
                        SharedUtils::DrawCurrentItemChrome(logsTabOpen, ImGui::IsItemHovered(), true, false);
                        if (logsTabOpen) {
                            activeMainTab = "Logs";
                            focusTabIfRequested("Logs");
                            const auto logs = LogService::Read();
                            LogRequests requests;
                            LogViewerTab::Draw(logViewer, *logs, requests, L);
                            LogService::Apply(requests, L("Logs", "sLogFiles", "Log Files"), L("Logs", "sAllFiles", "All Files"));
                            ImGui::EndTabItem();
                        }
                    }

                    ItemGrantPopup::Requests grantRequests;
                    itemGrantPopup.Draw({ L, session, ActionService::IsReady(), settings.fontSize }, grantRequests);
                    if (grantRequests.submit) itemGrantPopup.ResolveSubmit(grantRequests.submit->revision,
                        ActionService::SubmitBatch(std::move(grantRequests.submit->actions)));
                    MainWindowPopups::Requests popupRequests;
                    mainWindowPopups.Draw({ L, session, ActionService::IsReady(), settings.fontSize, SettingsService::Read().toggleKeyName }, popupRequests);
                    for (auto& submission : popupRequests.submissions) {
                        const auto admission = ActionService::SubmitBatch(std::move(submission.actions));
                        mainWindowPopups.ResolveSubmit(submission.revision, admission);
                        if (submission.origin == MainWindowPopups::Origin::Inventory) inventoryBrowser.admission = admission;
                    }
                    if (popupRequests.helpDismissed && !Config::Get().firstRunHelpDismissed) {
                        auto updated = Config::Get();
                        updated.firstRunHelpDismissed = true;
                        SettingsService::Apply(std::move(updated), false);
                    }

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
                        FavoriteService::Forms().size(),
                        fps, L("General", "sFPS", "FPS"), frameTime);
                } else {
                    ImGui::Text("%s: %zu  %s: %zu  %s: %zu",
                        L("PluginBrowser", "sPluginsCount", "Plugins"),
                        plugins.size(),
                        L("General", "sForms", "Forms"),
                        totalForms,
                        L("General", "sFavorites", "Favorites"),
                        FavoriteService::Forms().size());
                }

                if (GamepadInput::IsGamepadConnected()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();
                    ImGui::TextDisabled("[%s]", L("Settings", "sGamepadStatus", "Gamepad"));
                }

                if (settings.showPlayerStatsInStatus) {
                    const auto player = PlayerStatusService::Request(session);

                    ImGui::SameLine();
                    ImGui::TextDisabled("|");
                    ImGui::SameLine();

                    if (player.ready && player.session == session) {
                        ImGui::TextDisabled("%s %d  %s %lld  %s %.0f  %s %.0f", L("General", "sLevelShort", ""), player.level, L("Inventory", "sCaps", ""), player.caps, L("Inventory", "sHealthShort", ""), player.health, L("Inventory", "sActionPointsShort", ""), player.actionPoints);
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
                ImGui::BeginDisabled(true);
                ImGui::Button(L("General", "sUndoLastAction", "Undo Last Action"));
                ImGui::EndDisabled();

                if (settings.showMenuResolutionInStatus) {
                    ImGui::SetCursorPosY(statusStartY + statusRowHeight + 2.0f);
                    ImGui::TextDisabled("%s: %dx%d", L("Settings", "sResolutionShort", "Res"), static_cast<int>(menuWindowSize.x), static_cast<int>(menuWindowSize.y));
                }
            }
            ImGui::EndChild();

            FavoriteService::CommitEdits(favoritesBefore);

            if (activeMainTab != previousActiveTab && Config::Get().autoFocusSearchBars) {
                tabSearchFocusPending = true;
            }

            if (activeMainTab != previousActiveTab && !activeMainTab.empty()) {
                REX::DEBUG("{}", "Active tab changed to: " + activeMainTab);
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
            REX::DEBUG("{}", "Main window close button pressed");
            Hooks::SetMenuVisible(false);
        }
    }
}
