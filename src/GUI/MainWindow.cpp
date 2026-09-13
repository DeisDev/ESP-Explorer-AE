#include "GUI/MainWindow.h"
#include "GUI/Widgets/MenuChrome.h"
#include "GUI/Widgets/RecordInspector.h"
#include "GUI/Widgets/WorkspaceView.h"
#include "GUI/Widgets/BasketView.h"
#include "GUI/Widgets/CommandPalette.h"
#include "GUI/Widgets/ComparisonView.h"
#include "App/WorkspaceService.h"
#include "Core/WorkspaceDestination.h"
#include "GUI/Tabs/PlayerWorldTab.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Icons.h"

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
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/ActionHistoryView.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/ItemGrantPopup.h"
#include "GUI/Widgets/MainWindowPopups.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Tabs/PluginBrowserPanels.h"
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
        ActionHistoryViewState actionHistory;
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
        PlayerWorldState playerWorld;
        RecordInspectorState inspector;
        std::vector<RecordInspectorState> pinnedInspectors;
        std::vector<std::uint32_t> pendingPins;
        bool inspectorTool{};
        bool sourcesOpen{ true };
        bool sourcesDrawer{};
        bool inspectorPaneOpen{ true };
        float sourcesWidth{ 220.0f };
        float inspectorWidth{ 380.0f };
        std::array<char, 128> sourceSearch{};
        WorkspaceViewState workspaceView;
        ItemKit basket;
        bool basketOpen{};
        BasketViewState basketView;
        CommandPaletteState commandPalette;
        ComparisonViewState comparison;
        bool diagnosticsOpen{};
        bool diagnosticsFocus{};
        bool discardPopupStack{};
        bool refreshDataRequested{ false };
        bool refreshDataInProgress{ false };

        void ResetInspectionSession()
        {
            inventoryDetailRequest.reset();
            inspector = {};
            pinnedInspectors.clear();
            pendingPins.clear();
            inspectorTool = false;
            comparison = {};
            commandPalette = {};
            basketView.reviewed.reset();
            basketView.submitted = false;
            workspaceView.collect.clear();
            playerWorld = {};
        }

        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::FrameText(section, key);
            return value.empty() ? fallback : value.data();
        }

        void ResetCurrentView()
        {
            if (activeMainTab == "Inventory") { InventoryTab::ResetFilters(inventoryBrowser); return; }
            if (activeMainTab == "Plugin Browser") {
                pluginBrowser.search.clear();
                pluginBrowser.scope = {};
                pluginBrowser.structuredSearch = false;
                pluginBrowser.searchBuffer.fill(0);
                pluginBrowser.selection.Clear();
                pluginBrowser.focusPending = true;
                return;
            }
            BrowserState* state = activeMainTab == "Item Browser" ? &itemBrowser :
                activeMainTab == "NPC Browser" ? &npcBrowser.browser : activeMainTab == "Cell Browser" ? &cellBrowser :
                activeMainTab == "Object Browser" ? &objectBrowser : activeMainTab == "Spells & Perks" ? &spellPerkBrowser : nullptr;
            if (!state) return;
            state->search.clear();
            state->scope = {};
            state->structuredSearch = false;
            state->searchBuffer.fill(0);
            state->focusPending = true;
            for (auto& [id, rows] : state->categories) { rows.table.selection.Clear(); rows.table.sort = {}; rows.query.Clear(); }
            if (state == &npcBrowser.browser) npcBrowser.filters = {};
        }

        RecordScope& ActiveScope()
        {
            if (activeMainTab == "Item Browser") return itemBrowser.scope;
            if (activeMainTab == "NPC Browser") return npcBrowser.browser.scope;
            if (activeMainTab == "Cell Browser") return cellBrowser.scope;
            if (activeMainTab == "Object Browser") return objectBrowser.scope;
            if (activeMainTab == "Spells & Perks") return spellPerkBrowser.scope;
            return pluginBrowser.scope;
        }

        void DrawPluginFilterStatus()
        {
            const auto& scope = ActiveScope();
            const auto* label = scope.kind == SearchScope::AllPlugins ? L("Search", "sAllPlugins", "All Plugins") :
                scope.kind == SearchScope::SelectedPlugins ? L("Search", "sSelectedPlugins", "Selected Plugins") : L("Search", "sCollection", "Collection");
            ImGui::TextDisabled("%s", label);
        }

        WorkspaceLocation CaptureLocation();
        void RestoreLocation(const WorkspaceLocation& location);
        BrowserView InspectorContext(std::shared_ptr<const CatalogSnapshot> catalog);
        void HandleWorkspaceRequests(WorkspaceViewRequests& requests);

        void DrawSources(const CatalogSnapshot& catalog)
        {
            ImGui::TextUnformatted(L("General", "sSourcesViews", "Sources/Views"));
            auto& scope = ActiveScope();
            WorkspaceViewRequests requests;
            DrawWorkspaceSources(workspaceView, *WorkspaceService::Read(), scope, InspectorContext(CatalogService::Read()), requests);
            HandleWorkspaceRequests(requests);
            if (ImGui::Selectable(L("Search", "sAllPlugins", "All Plugins"), scope.kind == SearchScope::AllPlugins)) scope.kind = SearchScope::AllPlugins;
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##SourceSearch", L("General", "sSearch", "Search"), sourceSearch.data(), sourceSearch.size());
            SearchBar::ReadControllerText(L("General", "sSearch", "Search"), sourceSearch.data(), sourceSearch.size());
            if (ImGui::BeginChild("SourcePlugins", {0, 0})) {
                for (const auto& plugin : catalog.plugins) {
                    if (!SearchContains(plugin.filename, sourceSearch.data())) continue;
                    const auto found = std::ranges::find(scope.plugins, plugin.filename);
                    bool selected = found != scope.plugins.end();
                    if (ImGui::Checkbox(plugin.filename.c_str(), &selected)) {
                        pluginBrowser.diagnosticsPlugin = plugin.filename;
                        scope.kind = SearchScope::SelectedPlugins;
                        if (selected) scope.plugins.push_back(plugin.filename);
                        else scope.plugins.erase(found);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s | %s | %u", plugin.filename.c_str(), plugin.type.c_str(), plugin.runtimeSourceRecords);
                }
            }
            ImGui::EndChild();
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
            std::array<std::string, 9> labels;
            std::array<std::string, 9> counts;
        };

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
            const auto count = [](RecordCount value) {
                return value.filtered < value.total ? std::to_string(value.filtered) + " / " + std::to_string(value.total) : std::to_string(value.total);
            };
            cache.labels = { L("PluginBrowser", "sBrowserTab", "Plugin Browser"), L("Inventory", "sTabName", "Inventory"),
                L("Items", "sBrowserTab", "Item Browser"), L("NPCs", "sBrowserTab", "NPC Browser"), L("Cells", "sBrowserTab", "Cell Browser"),
                L("Objects", "sBrowserTab", "Object Browser"), L("Spells", "sBrowserTab", "Spells & Perks"),
                L("Settings", "sTabName", "Settings"), L("Logs", "sTabName", "Logs") };
            cache.counts = { std::to_string(catalog.plugins.size()), "",
                count(counts.For({"WEAP", "ARMO", "AMMO", "MISC", "KEYM", "NOTE", "BOOK", "ALCH", "CMPO"})),
                count(counts.For({"NPC_"})), count(counts.For({"CELL"})), count(counts.For({"ACTI", "CONT", "STAT", "FURN"})),
                count(counts.For({"SPEL", "PERK"})), "", "" };
            return cache;
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
                    return settings.lastActiveTab == "Player" ? "Player & World" : settings.lastActiveTab;
                }
                return "Plugin Browser";
            }

            if (!settings.startupTab.empty()) {
                return settings.startupTab == "Player" ? "Player & World" : settings.startupTab;
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
            if (activeMainTab == "Explore" || activeMainTab == "Tools") activeMainTab = DefaultDestinationPage(DestinationFor(activeMainTab));
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

            const auto& settings = Config::Get();
            itemGrantPopup.Open(entry, ActionService::Session(), settings.includeAmmoWithWeapons, settings.defaultAmmoQuantity);
        }


        BrowserState* ActiveBrowser()
        {
            if (activeMainTab == "Item Browser") return &itemBrowser;
            if (activeMainTab == "NPC Browser") return &npcBrowser.browser;
            if (activeMainTab == "Cell Browser") return &cellBrowser;
            if (activeMainTab == "Object Browser") return &objectBrowser;
            if (activeMainTab == "Spells & Perks") return &spellPerkBrowser;
            return nullptr;
        }

        WorkspaceLocation CaptureLocation()
        {
            WorkspaceLocation location;
            location.page = activeMainTab;
            location.query.scope = ActiveScope();
            location.sourceWidth = sourcesWidth;
            location.inspectorWidth = inspectorWidth;
            location.sourcesOpen = sourcesOpen;
            location.inspectorOpen = inspectorPaneOpen;
            if (activeMainTab == "Inventory") {
                location.inventory = InventoryTab::CaptureLocation(inventoryBrowser);
                location.query.search = inventoryBrowser.inventorySearch;
                location.category = std::to_string(location.inventory->category);
            } else if (activeMainTab == "Plugin Browser") {
                location.query.search = pluginBrowser.search;
                location.query.structuredSearch = pluginBrowser.structuredSearch;
                location.allRuntimeRecords = pluginBrowser.allRuntimeRecords;
                location.resultsScroll = pluginBrowser.scroll;
                location.active = pluginBrowser.selection.records.active;
                location.selection.assign(pluginBrowser.selection.records.selected.begin(), pluginBrowser.selection.records.selected.end());
            } else if (auto* browser = ActiveBrowser()) {
                location.query.search = browser->search;
                location.query.structuredSearch = browser->structuredSearch;
                location.category = browser->activeCategory;
                if (const auto found = browser->categories.find(location.category); found != browser->categories.end()) {
                    const auto& table = found->second.table;
                    location.query.sortColumn = table.sort.column;
                    location.query.ascending = table.sort.ascending;
                    location.active = table.selection.active;
                    location.selection.assign(table.selection.selected.begin(), table.selection.selected.end());
                    location.resultsScroll = table.scroll;
                    location.columns = table.columns;
                }
                if (browser == &npcBrowser.browser) location.query.npc = npcBrowser.filters;
            }
            return location;
        }

        void RestoreLocation(const WorkspaceLocation& location)
        {
            activeMainTab = location.page;
            requestedMainTab = location.page;
            if (DestinationFor(location.page) == WorkspaceDestination::Explore) {
                ActiveScope() = location.query.scope;
                ResolveCollectionScope(ActiveScope(), *WorkspaceService::Read(), *CatalogService::Read(), ActionService::Session());
            }
            sourcesWidth = location.sourceWidth;
            inspectorWidth = location.inspectorWidth;
            sourcesOpen = location.sourcesOpen;
            inspectorPaneOpen = location.inspectorOpen;
            if (location.page == "Inventory" && location.inventory) InventoryTab::RestoreLocation(inventoryBrowser, *location.inventory);
            else if (location.page == "Plugin Browser") {
                pluginBrowser.search = location.query.search;
                pluginBrowser.structuredSearch = location.query.structuredSearch;
                pluginBrowser.allRuntimeRecords = location.allRuntimeRecords;
                std::snprintf(pluginBrowser.searchBuffer.data(), pluginBrowser.searchBuffer.size(), "%s", pluginBrowser.search.c_str());
                pluginBrowser.selection.Single({"History", location.active});
                pluginBrowser.selection.records.selected = {location.selection.begin(), location.selection.end()};
                pluginBrowser.scroll = location.resultsScroll;
                pluginBrowser.restoreScroll = true;
            } else if (auto* browser = ActiveBrowser()) {
                browser->search = location.query.search;
                browser->structuredSearch = location.query.structuredSearch;
                std::snprintf(browser->searchBuffer.data(), browser->searchBuffer.size(), "%s", browser->search.c_str());
                browser->activeCategory = location.category;
                browser->requestedCategory = location.category;
                auto& rows = browser->categories[location.category];
                rows.table.sort = {location.query.sortColumn, location.query.ascending};
                rows.table.selection.Single(location.active);
                rows.table.selection.selected = {location.selection.begin(), location.selection.end()};
                rows.table.scroll = location.resultsScroll;
                rows.table.restoreScroll = true;
                rows.table.columns = location.columns;
                rows.table.restoreColumns = true;
                rows.table.orderRevision.reset();
                rows.query.Clear();
                if (browser == &npcBrowser.browser) npcBrowser.filters = location.query.npc;
            }
        }

        void OpenInspector(std::uint32_t id)
        {
            if (!CatalogService::Read()->Find(id)) return;
            auto location = CaptureLocation();
            if (auto* current = inspector.history.Current(); current && current->workspace.page == location.page &&
                current->workspace.category == location.category && current->workspace.query.search == location.query.search &&
                current->workspace.query.structuredSearch == location.query.structuredSearch && current->workspace.query.scope == location.query.scope)
                current->workspace = location;
            inspector.history.Open(id, std::move(location));
            inspector.restoreScroll = true;
            inspector.open = true;
            inspectorTool = activeMainTab == "Inventory";
        }

        void HandleWorkspaceRequests(WorkspaceViewRequests& requests)
        {
            if (requests.restore) RestoreLocation(*requests.restore);
            for (auto id : requests.inspect) { OpenInspector(id); inspectorTool = true; inspector.focusPending = true; }
            if (requests.loadKit) {
                if (basket.entries.size() + requests.loadKit->entries.size() > ActionQueue::Capacity) {
                    basketView.full = true; basketOpen = true; return;
                }
                if (basket.entries.empty()) {
                    basket = std::move(*requests.loadKit);
                    std::snprintf(basketView.name.data(), basketView.name.size(), "%s", basket.name.c_str());
                }
                else basket.entries.insert(basket.entries.end(), requests.loadKit->entries.begin(), requests.loadKit->entries.end());
                basketOpen = true; basketView.submitted = false; basketView.reviewed.reset(); basketView.focusPending = true;
            }
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
                const auto& settings = Config::Get();
                itemGrantPopup.Open(entries, grant.session, settings.includeAmmoWithWeapons, settings.defaultAmmoQuantity);
            }
            for (const auto& global : requests.globalValues) {
                if (global.session != ActionService::Session()) { admission = ActionAdmission::StaleSession; continue; }
                if (!ActionService::IsReady()) { admission = ActionAdmission::Unavailable; continue; }
                const auto catalog = CatalogService::Read();
                const auto* record = catalog->Find(global.formID);
                if (!record || !PrepareRecordAction(ActionKind::SetGlobal, *record, global.session, true)) { admission = ActionAdmission::Invalid; continue; }
                mainWindowPopups.OpenGlobalValuePopup(global.formID, record->editorID, global.session);
            }
            for (auto id : requests.recentSelections) { TrackRecentRecord(id); OpenInspector(id); }
            for (auto id : requests.inspections) OpenInspector(id);
            pendingPins.insert(pendingPins.end(), requests.pins.begin(), requests.pins.end());
            const auto workspaceCatalog = CatalogService::Read();
            const auto workspaceSession = ActionService::Session();
            for (auto id : requests.pinA) if (const auto* record = workspaceCatalog->Find(id)) {
                comparison.a = CompareBaseRecord(*record, workspaceSession, workspaceCatalog->generation);
                comparison.open = true; comparison.focusPending = true;
            }
            for (auto id : requests.compareB) if (const auto* record = workspaceCatalog->Find(id)) {
                comparison.b = CompareBaseRecord(*record, workspaceSession, workspaceCatalog->generation);
                comparison.open = true; comparison.focusPending = true;
            }
            if (!requests.collections.empty()) {
                workspaceView.collect = requests.collections;
                workspaceView.open = true; workspaceView.focusPending = true;
            }
            for (auto id : requests.basket) {
                auto record = CaptureWorkspaceRecord(*workspaceCatalog, id, workspaceSession);
                const auto found = std::ranges::find_if(basket.entries, [&](const auto& entry) {
                    return record.identity.empty() ? entry.record.identity.empty() && entry.record.transientID == id && entry.record.session == workspaceSession : entry.record.identity == record.identity;
                });
                if (found == basket.entries.end()) {
                    if (basket.entries.size() < ActionQueue::Capacity) {
                        const auto* entry = workspaceCatalog->Find(id);
                        basket.entries.push_back({std::move(record), 1, entry && entry->category == "WEAP" && Config::Get().includeAmmoWithWeapons, static_cast<std::uint32_t>((std::max)(0, Config::Get().defaultAmmoQuantity))});
                    } else basketView.full = true;
                }
                basketView.reviewed.reset(); basketView.submitted = false;
                basketOpen = true;
            }
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

        void DrawInspector(RecordInspectorState& state, const BrowserView& view, bool restoreWorkspace)
        {
            const auto* current = state.history.Current();
            const bool advanced = Config::Get().pluginAdvancedDetailsView;
            auto details = current ? DetailService::Request({current->formID, view.session, view.catalog->generation, advanced}) : nullptr;
            bool outside{};
            if (current && restoreWorkspace) {
                if (activeMainTab == "Plugin Browser") outside = !pluginBrowser.query.Result().eligibleIDs.contains(current->formID);
                else if (auto* browser = ActiveBrowser()) {
                    const auto found = browser->categories.find(browser->activeCategory);
                    outside = found == browser->categories.end() || !found->second.table.displayedPositions.contains(current->formID);
                }
            }
            RecordInspectorRequests requests;
            state.canPin = pinnedInspectors.size() + pendingPins.size() < 3;
            DrawRecordInspector(state, view, std::move(details), advanced, outside, requests);
            HandleBrowserRequests(requests.records, pluginBrowser.admission);
            pendingPins.insert(pendingPins.end(), requests.pins.begin(), requests.pins.end());
            if (requests.open) {
                if (restoreWorkspace) OpenInspector(*requests.open);
                else { state.history.Open(*requests.open, CaptureLocation()); state.restoreScroll = true; }
            }
            if (requests.historyMove) {
                if (auto* currentLocation = state.history.Current(); currentLocation && restoreWorkspace) {
                    const auto location = CaptureLocation();
                    if (currentLocation->workspace.page == location.page && currentLocation->workspace.category == location.category &&
                        currentLocation->workspace.query.search == location.query.search && currentLocation->workspace.query.scope == location.query.scope)
                        currentLocation->workspace = location;
                }
                const bool moved = requests.historyMove < 0 ? state.history.Back() : state.history.Forward();
                if (moved) {
                    state.restoreScroll = true;
                    if (restoreWorkspace) RestoreLocation(state.history.Current()->workspace);
                }
            }
        }

        BrowserView InspectorContext(std::shared_ptr<const CatalogSnapshot> catalog)
        {
            const auto& config = Config::Get();
            return { std::move(catalog), browserFilters, FavoriteService::Forms(), selectedPluginFilter,
                ActionService::Session(), ActionService::IsReady(), L, {}, config.autoFocusSearchBars,
                static_cast<std::uint32_t>((std::max)(0, inventoryBrowser.quick.currentAmmo)), config.multiCopyFormat, config.doubleClickGameplayAction, config.compactTableDensity, config.componentSubstitution };
        }

        void DrawInventoryTab(std::shared_ptr<const CatalogSnapshot> catalog)
        {
            const auto session = ActionService::Session();
            const bool advanced = Config::Get().pluginAdvancedDetailsView;
            InventoryTabView view{ .localize = L, .catalog = std::move(catalog), .inventory = InventoryService::Request(session),
                .session = session, .gameplayReady = ActionService::IsReady(), .godMode = ActionService::GodModeEnabled(), .advancedDetails = advanced, .doubleClickGameplayAction = Config::Get().doubleClickGameplayAction };
            if (inventoryDetailRequest && inventoryDetailRequest->session == session && inventoryDetailRequest->catalogGeneration == view.catalog->generation && inventoryDetailRequest->advanced == advanced)
                view.details = DetailService::Request(*inventoryDetailRequest);
            if (tabSearchFocusPending) {
                inventoryBrowser.focusPending = true;
                tabSearchFocusPending = false;
            }
            InventoryTabRequests requests;
            InventoryTab::Draw(inventoryBrowser, view, requests);
            HandleBrowserRequests(requests.records, inventoryBrowser.admission);
            if (requests.pinA) { comparison.a = std::move(requests.pinA); comparison.open = true; comparison.focusPending = true; }
            if (requests.compareB) { comparison.b = std::move(requests.compareB); comparison.open = true; comparison.focusPending = true; }
            for (auto& confirmation : requests.confirmations) {
                mainWindowPopups.RequestActionConfirmation(std::move(confirmation.title), std::move(confirmation.message),
                    std::move(confirmation.actions), MainWindowPopups::Origin::Inventory);
            }
            if (requests.refresh) InventoryService::Request(session, true);
            inventoryDetailRequest = requests.details;
            if (requests.details) DetailService::Request(*requests.details);
            if (requests.inspect) { OpenInspector(*requests.inspect); inspectorTool = true; }

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
                    static_cast<std::uint32_t>((std::max)(0, inventoryBrowser.quick.currentAmmo)), config.multiCopyFormat, config.doubleClickGameplayAction, config.compactTableDensity },
                .advancedDetails = config.pluginAdvancedDetailsView,
                .details = {},
                .recentLimit = static_cast<std::size_t>((std::clamp)(config.recentRecordsLimit, 5, 100)),
                .copyFormat = config.multiCopyFormat,
                .favoriteReviewCount = FavoriteService::Review().legacy.size() + FavoriteService::Review().unresolved.size()
            };
            if (const auto id = pluginBrowser.selection.records.active; id && snapshot->Find(id)) {
                view.details = DetailService::Request({ id, view.records.session, snapshot->generation, view.advancedDetails });
            }
            view.drawInspector = [&] { DrawInspector(inspector, view.records, true); };
            const auto availableWidth = ImGui::GetContentRegionAvail().x;
            view.showInspector = inspectorPaneOpen && availableWidth >= 720.0f;
            view.inspectorWidth = inspectorWidth;
            PluginBrowserRequests requests;
            PluginBrowserTab::Draw(pluginBrowser, view, requests);
            if (requests.resultsWidth && view.showInspector) inspectorWidth = availableWidth - *requests.resultsWidth - ImGuiWidgetUtils::PaneDividerSize();
            HandleBrowserRequests(requests.records, pluginBrowser.admission);
            if (pluginBrowser.selection.records.active && pluginBrowser.selection.records.active != pluginBrowser.detailsFormID) {
                pluginBrowser.detailsFormID = pluginBrowser.selection.records.active;
                OpenInspector(pluginBrowser.detailsFormID);
            }
            if (requests.pluginFilter) selectedPluginFilter = std::move(*requests.pluginFilter);
            if (requests.settingsChanged) PersistFilterCheckboxSettings();
            if (requests.details) DetailService::Request(*requests.details);
            inspectorTool = inspectorPaneOpen && !view.showInspector && inspector.history.Current();
        }

        template <class Draw>
        void DrawCatalogBrowser(BrowserState& browser, std::shared_ptr<const CatalogSnapshot> catalog, Draw&& draw)
        {
            if (tabSearchFocusPending) {
                browser.focusPending = true;
                tabSearchFocusPending = false;
            }
            BrowserRequests requests;
            auto view = InspectorContext(catalog);
            view.drawPluginFilterStatus = DrawPluginFilterStatus;
            const float width = ImGui::GetContentRegionAvail().x;
            const bool sideBySide = inspectorPaneOpen && width >= 720.0f;
            if (sideBySide) inspectorTool = false;
            const float divider = ImGuiWidgetUtils::PaneDividerSize();
            const float minimumResults = (std::min)(ImGui::GetFontSize() * 24.0f, width * 0.50f);
            const float maximumResults = (std::max)(minimumResults, width - (std::min)(ImGui::GetFontSize() * 20.0f, width * 0.45f) - divider);
            float resultsWidth = sideBySide ? (std::clamp)(width - inspectorWidth - divider, minimumResults, maximumResults) : width;
            if (ImGui::BeginChild("BrowserResults", {resultsWidth, 0})) draw(browser, view, requests);
            ImGui::EndChild();
            HandleBrowserRequests(requests, browser.admission);
            if (sideBySide) {
                ImGui::SameLine(0, 0);
                ImGuiWidgetUtils::PaneDivider("##BrowserResultsDivider", resultsWidth, minimumResults, maximumResults, L("General", "sResizePanes", "Drag to resize panes"));
                inspectorWidth = width - resultsWidth - divider;
                ImGui::SameLine(0, 0);
                if (ImGui::BeginChild("BrowserInspector", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) DrawInspector(inspector, view, true);
                ImGui::EndChild();
            } else if (inspectorPaneOpen && inspector.history.Current()) inspectorTool = true;

        }


    }

    void MainWindow::ResetStateFromConfig()
    {
        const auto& settings = Config::Get();
        itemGrantPopup.Close();
        actionHistory = {};
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
        ResetInspectionSession();

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
        const auto startup = activeMainTab;
        const auto storedWorkspace = WorkspaceService::Read();
        for (const auto& saved : storedWorkspace->layouts) {
            activeMainTab = saved.location.page;
            if (auto* browser = ActiveBrowser()) {
                auto& table = browser->categories[saved.location.category].table;
                table.columns = saved.location.columns;
                table.restoreColumns = true;
                table.sort = {saved.location.query.sortColumn, saved.location.query.ascending};
            }
            sourcesWidth = saved.location.sourceWidth;
            inspectorWidth = saved.location.inspectorWidth;
            sourcesOpen = saved.location.sourcesOpen;
            inspectorPaneOpen = saved.location.inspectorOpen;
        }
        activeMainTab = startup;
        previousMainTab.clear();
        tabSearchFocusPending = settings.autoFocusSearchBars;
        pluginBrowser.collapseDiagnostics = false;
    }

    void MainWindow::Shutdown()
    {
        // Render-owner cleanup only; no config reset or gameplay effects.
        itemGrantPopup.Close();
        actionHistory = {};
        mainWindowPopups.HandleMenuVisibilityChanged(false);
        settingsTab = {};
        logViewer = {};
        LogService::Reset();
        pluginBrowser = {};
        inventoryBrowser = {};
        ResetInspectionSession();
        workspaceView = {};
        basket = {};
        basketView = {};
        basketOpen = false;
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
        if (visible && actionHistory.open) actionHistory.focusPending = true;
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
            actionHistory = {};
            mainWindowPopups.HandleMenuVisibilityChanged(false);
            itemGrantPopup.Close();
            settingsTab.Close();
            InventoryTab::ResetState(inventoryBrowser);
            ResetInspectionSession();
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
        static std::shared_ptr<const WorkspaceDocument> resolvedWorkspace;
        static std::uint64_t resolvedGeneration{};
        const auto workspace = WorkspaceService::Read();
        const auto currentCatalog = CatalogService::Read();
        if (workspace != resolvedWorkspace || currentCatalog->generation != resolvedGeneration) {
            resolvedWorkspace = workspace; resolvedGeneration = currentCatalog->generation;
            for (auto* scope : {&pluginBrowser.scope, &itemBrowser.scope, &npcBrowser.browser.scope, &cellBrowser.scope, &objectBrowser.scope, &spellPerkBrowser.scope})
                ResolveCollectionScope(*scope, *workspace, *currentCatalog, session);
        }

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
            (std::min)(960.0f * windowScale, (std::max)(viewportSize.x, 1.0f)),
            (std::min)(600.0f * windowScale, (std::max)(viewportSize.y, 1.0f)));
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
        ImGui::SetNextWindowSizeConstraints(minWindowSize, maxSavedWindowSize);

        const auto title = Language::FrameText("General", "sWindowTitle");
        const auto* windowTitle = title.empty() ? "ESP Explorer AE" : title.data();

        bool windowOpen = true;
        if (ImGui::Begin(windowTitle, &windowOpen, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            if (MenuChrome::Header(windowTitle, L("General", "sWorkspaceCaption", "Load order workspace"), L("General", "sCloseMenu", "Close menu"))) windowOpen = false;
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
            const bool shortcutsAllowed = !io.WantTextInput && !ImGui::IsAnyItemActive() && !GamepadInput::IsSteamKeyboardOpen() &&
                !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
            if (shortcutsAllowed && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) commandPalette.requested = true;
            if (shortcutsAllowed && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
                tabSearchFocusPending = true;
            }

            const auto& style = ImGui::GetStyle();
            const float footerTextRows = settings.showPlayerStatsInStatus ? 2.0f : 1.0f;
            const float rawFooterHeight = (std::max)(ImGui::GetTextLineHeightWithSpacing() * (footerTextRows + (settings.showMenuResolutionInStatus ? 1.0f : 0.0f)), ImGui::GetFrameHeight()) + style.WindowPadding.y * 2.0f + style.CellPadding.y * 2.0f + style.ItemSpacing.y;
            const float footerHeight = (std::min)(rawFooterHeight, (std::max)(0.0f, ImGui::GetContentRegionAvail().y - 1.0f));
            if (ImGui::BeginChild("MainContentRegion", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (!settings.showLogsTab && requestedMainTab == "Logs") requestedMainTab = "Settings";
                if (settings.enableGamepadNav && shortcutsAllowed && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && (GamepadInput::WasTabNextPressed() || GamepadInput::WasTabPrevPressed())) {
                    auto index = static_cast<int>(DestinationFor(activeMainTab));
                    index = (index + (GamepadInput::WasTabNextPressed() ? 1 : 3)) % 4;
                    requestedMainTab = DefaultDestinationPage(static_cast<WorkspaceDestination>(index));
                }
                if (!requestedMainTab.empty()) {
                    if (std::ranges::find(kMainTabOrder, requestedMainTab) != kMainTabOrder.end() || requestedMainTab == "Player & World") activeMainTab = requestedMainTab;
                    requestedMainTab.clear();
                }

                const std::array destinationLabels{L("General", "sExplore", "Explore"), L("Inventory", "sTabName", "Inventory"),
                    L("General", "sPlayerWorld", "Player & World"), L("General", "sNavigationTools", "Tools")};
                const auto previousDestination = DestinationFor(activeMainTab);
                const auto selectedDestination = static_cast<WorkspaceDestination>(MenuChrome::Destinations(destinationLabels, static_cast<int>(previousDestination)));
                if (selectedDestination != previousDestination) activeMainTab = DefaultDestinationPage(selectedDestination);
                const auto destination = DestinationFor(activeMainTab);
                std::vector<MenuChrome::ToolbarPage> toolbarPages;
                if (destination == WorkspaceDestination::Explore || destination == WorkspaceDestination::Tools) {
                    for (std::size_t index = 0; index < kMainTabOrder.size(); ++index) {
                        const auto page = kMainTabOrder[index];
                        if (page == "Inventory" || DestinationFor(page) != destination || (page == "Logs" && !settings.showLogsTab)) continue;
                        toolbarPages.push_back({page.data(), tabLabels.labels[index].c_str(), activeMainTab == page});
                    }
                }
                const bool wideWorkspace = ImGui::GetContentRegionAvail().x >= 1000.0f;
                const auto toolbar = MenuChrome::WorkspaceToolbar(toolbarPages, basket.entries.size(), destination == WorkspaceDestination::Explore,
                    wideWorkspace ? sourcesOpen : sourcesDrawer, inspectorPaneOpen, refreshDataRequested || refreshDataInProgress, L);
                if (!toolbar.page.empty()) activeMainTab = toolbar.page;
                if (toolbar.commands) commandPalette.requested = true;
                if (toolbar.basket) { basketOpen = true; basketView.focusPending = true; }
                if (toolbar.compare) { comparison.open = true; comparison.focusPending = true; }
                if (toolbar.sources) {
                    if (wideWorkspace) sourcesOpen = !sourcesOpen;
                    else sourcesDrawer = !sourcesDrawer;
                }
                if (toolbar.inspector) inspectorPaneOpen = !inspectorPaneOpen;
                if (toolbar.diagnostics) { diagnosticsOpen = true; diagnosticsFocus = true; }
                if (toolbar.refresh) refreshDataRequested = true;
                if (toolbar.reset) ResetCurrentView();
                if (DestinationFor(activeMainTab) == WorkspaceDestination::Explore) {
                    const bool sourcePane = sourcesOpen && ImGui::GetContentRegionAvail().x >= 1000.0f;
                    if (sourcePane) {
                        const float minimum = ImGui::GetFontSize() * 10.0f;
                        const float maximum = (std::max)(minimum, ImGui::GetContentRegionAvail().x * 0.35f);
                        sourcesWidth = (std::clamp)(sourcesWidth, minimum, maximum);
                        if (ImGui::BeginChild("WorkspaceSources", {sourcesWidth, 0}, ImGuiChildFlags_Borders)) DrawSources(*catalog);
                        ImGui::EndChild();
                        ImGui::SameLine(0, 0);
                        ImGuiWidgetUtils::PaneDivider("##SourcesDivider", sourcesWidth, minimum, maximum, L("General", "sResizePanes", "Drag to resize panes"));
                        ImGui::SameLine(0, 0);
                    }
                    if (sourcesDrawer) {
                        const auto drawerTitle = std::string(L("General", "sSourcesViews", "Sources/Views")) + "###SourcesDrawer";
                        bool focus = false;
                        ModalUtils::PrepareToolWindow(drawerTitle.c_str(), {320, 550}, {200, 200}, focus);
                        if (ImGui::Begin(drawerTitle.c_str(), &sourcesDrawer)) {
                            DrawSources(*catalog);
                            if (ModalUtils::EscapeClosesCurrentWindow()) sourcesDrawer = false;
                        }
                        ImGui::End();
                    }
                }
                if (ImGui::BeginChild("MainWorkspace", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened)) {
                    for (std::size_t i = 0; i < kMainTabOrder.size(); ++i) {
                        if (activeMainTab == kMainTabOrder[i]) MenuChrome::PageHeading(tabLabels.labels[i].c_str(), tabLabels.counts[i].c_str());
                    }
                    if (activeMainTab == "Plugin Browser") {
                        if (refreshDataRequested || refreshDataInProgress) {
                            ImGui::TextUnformatted(L("General", "sRefreshingData", "Refreshing..."));
                        }
                        DrawPluginBrowser(catalog);
                    }

                    if (activeMainTab == "Player & World") {
                        BrowserRequests requests;
                        DrawPlayerWorld(playerWorld, InspectorContext(catalog), PlayerStatusService::Request(session), ActionService::GodModeEnabled(), requests);
                        HandleBrowserRequests(requests, playerWorld.admission);
                    }
                    if (activeMainTab == "Inventory") {
                        DrawInventoryTab(catalog);
                    }

                    if (activeMainTab == "Item Browser") {
                        DrawCatalogBrowser(itemBrowser, catalog, ItemBrowserTab::Draw);
                    }

                    if (activeMainTab == "NPC Browser") {
                        DrawCatalogBrowser(npcBrowser.browser, catalog, [](BrowserState&, const BrowserView& view, BrowserRequests& requests) {
                            NPCBrowserTab::Draw(npcBrowser, view, requests);
                        });
                    }

                    if (activeMainTab == "Cell Browser") {
                        DrawCatalogBrowser(cellBrowser, catalog, CellBrowserTab::Draw);
                    }

                    if (activeMainTab == "Object Browser") {
                        DrawCatalogBrowser(objectBrowser, catalog, ObjectBrowserTab::Draw);
                    }

                    if (activeMainTab == "Spells & Perks") {
                        DrawCatalogBrowser(spellPerkBrowser, catalog, SpellPerkBrowserTab::Draw);
                    }

                    if (activeMainTab == "Settings") {
                        SettingsTabRequests requests;
                        const auto favoriteReview = FavoriteService::Review();
                        const auto resources = SettingsService::Read();
                        SettingsTab::Draw(settingsTab, { Config::Get(), resources, favoriteReview, L }, requests);
                        SettingsService::Apply(std::move(requests.settings), requests.reloadThemes);
                        if (requests.resetAll) MainWindow::ResetStateFromConfig();
                        if (!requests.resetAll && !requests.favorites.accepted.empty() && !FavoriteService::AcceptLegacy(requests.favorites.review, requests.favorites.accepted)) settingsTab.favorites.stale = true;
                        if (requests.page) SettingsService::OpenPage(*requests.page);
                        if (requests.showHelp) mainWindowPopups.OpenHelpOverlay();
                    }

                    if (settings.showLogsTab) {
                        if (activeMainTab == "Logs") {
                            if (tabSearchFocusPending) {
                                logViewer.focusPending = true;
                                tabSearchFocusPending = false;
                            }
                            const auto logs = LogService::Read();
                            LogRequests requests;
                            LogViewerTab::Draw(logViewer, *logs, requests, L);
                            LogService::Apply(requests, L("Logs", "sLogFiles", "Log Files"), L("Logs", "sAllFiles", "All Files"));
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

                }
                ImGui::EndChild();
            }
            ImGui::EndChild();

            if (ImGui::BeginChild("StatusBarRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::BeginTable("##StatusLayout", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("##Status", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, CalcButtonWidth(L("General", "sActionHistory", "Action History")));
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    const float fps = io.Framerate;
                    const float frameTime = fps > 0.0f ? (1000.0f / fps) : 0.0f;

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

                        if (player.ready && player.session == session) {
                            ImGui::TextDisabled("%s %d  %s %lld  %s %.0f  %s %.0f", L("General", "sLevelShort", ""), player.level, L("Inventory", "sCaps", ""), player.caps, L("Inventory", "sHealthShort", ""), player.health, L("Inventory", "sActionPointsShort", ""), player.actionPoints);
                        } else {
                            ImGui::TextDisabled("%s --  %s --  %s --  %s --", L("General", "sLevelShort", ""), L("Inventory", "sCaps", ""), L("Inventory", "sHealthShort", ""), L("Inventory", "sActionPointsShort", ""));
                        }
                    }

                    if (settings.showMenuResolutionInStatus) {
                        ImGui::TextDisabled("%s: %dx%d", L("Settings", "sResolutionShort", "Res"), static_cast<int>(menuWindowSize.x), static_cast<int>(menuWindowSize.y));
                    }
                    ImGui::TableNextColumn();
                    if (ImGui::Button(L("General", "sActionHistory", "Action History"))) {
                        actionHistory.open = true;
                        actionHistory.focusPending = true;
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();

            if (workspaceView.open) {
                workspaceView.storageFailed = WorkspaceService::Writable() && !WorkspaceService::Error().empty();
                auto document = *WorkspaceService::Read();
                WorkspaceViewRequests requests;
                DrawWorkspaceWindow(workspaceView, document, CaptureLocation(), InspectorContext(catalog), WorkspaceService::Writable(), requests);
                if (requests.changed) workspaceView.failed = !WorkspaceService::Commit(std::move(document));
                HandleWorkspaceRequests(requests);
            }
            {
                std::vector<WorkspaceCommand> commands;
                const auto commandWorkspace = WorkspaceService::Read();
                for (std::size_t i = 0; i < kMainTabOrder.size(); ++i) {
                    if (kMainTabOrder[i] == "Logs" && !settings.showLogsTab) continue;
                    commands.push_back({WorkspaceCommandKind::Destination, tabLabels.labels[i], "", std::string(kMainTabOrder[i])});
                }
                commands.push_back({WorkspaceCommandKind::Destination, L("General", "sPlayerWorld", "Player & World"), "", "Player & World"});
                for (const auto& saved : commandWorkspace->views) commands.push_back({WorkspaceCommandKind::SavedView, saved.name, "", saved.name});
                commands.push_back({WorkspaceCommandKind::Basket, L("Workspace", "sBasket", "Basket")});
                commands.push_back({WorkspaceCommandKind::Collections, L("Workspace", "sManage", "Views & Collections")});
                commands.push_back({WorkspaceCommandKind::History, L("General", "sActionHistory", "Action History")});
                commands.push_back({WorkspaceCommandKind::AddSelection, L("Workspace", "sAddToBasket", "Add to Basket"), "", "",
                    !CaptureLocation().selection.empty(), L("Workspace", "sSelectResults", "Select records in the current results first.")});
                const auto currentComparison = [&]() -> std::optional<ComparisonRecord> {
                    if (activeMainTab == "Inventory") {
                        if (inventoryBrowser.snapshot) if (const auto* stack = inventoryBrowser.snapshot->Find(inventoryBrowser.detailsToken))
                            return CompareInventoryRecord(*stack, inventoryBrowser.snapshot->session, inventoryBrowser.snapshot->generation);
                        return {};
                    }
                    if (const auto* current = inspector.history.Current()) if (const auto* record = catalog->Find(current->formID)) return CompareBaseRecord(*record, session, catalog->generation);
                    return {};
                };
                const auto selected = currentComparison();
                commands.push_back({WorkspaceCommandKind::PinA, L("Comparison", "sPinA", "Pin A"), "", "", selected.has_value(), L("Comparison", "sChooseTargets", "Use Pin A on a record or chosen inventory instance, then Compare B on another target.")});
                commands.push_back({WorkspaceCommandKind::CompareB, L("Comparison", "sCompareB", "Compare B"), "", "", selected.has_value() && comparison.a.has_value(), L("Comparison", "sChooseTargets", "Use Pin A on a record or chosen inventory instance, then Compare B on another target.")});
                const auto reference = [&](const char* label, const char* binding, const char* explanation) { commands.push_back({WorkspaceCommandKind::Reference, label, binding, "", false, explanation}); };
                reference(L("Workspace", "sCommandPalette", "Commands & Shortcuts"), "Ctrl+K", L("Workspace", "sShortcutOwnership", "Bindings apply to the focused pane. Text edits, popups, and the Steam keyboard take priority."));
                reference(L("General", "sSearch", "Search"), "Ctrl+F", L("Workspace", "sFocusSearch", "Focus the current view's search field."));
                reference(L("General", "sInspect", "Inspect"), "Enter", L("Workspace", "sInspectShortcut", "Inspect the focused result without a gameplay action."));
                reference(L("General", "sActions", "Actions"), "Shift+F10 / X", L("Workspace", "sContextShortcut", "Open actions for the focused result."));
                reference(L("General", "sBack", "Back"), "Alt+Left / Alt+Right", L("Workspace", "sHistoryShortcut", "Move through inspector history and restore its workspace."));
                reference(L("General", "sSelectVisible", "Select Visible"), "Ctrl+A", L("Workspace", "sSelectShortcut", "Select all results in the focused table, including clipped rows."));
                reference(L("General", "sCopyFormID", "Copy FormID"), "Ctrl+C", L("Workspace", "sCopyShortcut", "Copy selected record IDs in the focused results table."));
                reference(L("General", "sCancel", "Cancel"), "Escape / B", L("Workspace", "sDismissShortcut", "Dismiss the innermost edit, popup, or tool before the overlay."));
                if (const auto command = DrawCommandPalette(commandPalette, commands, InspectorContext(catalog))) {
                    switch (command->kind) {
                    case WorkspaceCommandKind::Destination: requestedMainTab = command->argument; break;
                    case WorkspaceCommandKind::SavedView:
                        for (const auto& saved : commandWorkspace->views) if (saved.name == command->argument) RestoreLocation(ResolveSavedView(saved, *catalog, session));
                        break;
                    case WorkspaceCommandKind::Inspect:
                        if (const auto id = ParseExactFormID(command->argument)) { OpenInspector(*id); inspectorTool = true; inspector.focusPending = true; }
                        break;
                    case WorkspaceCommandKind::Basket: basketOpen = true; basketView.focusPending = true; break;
                    case WorkspaceCommandKind::Collections: workspaceView.open = true; workspaceView.focusPending = true; break;
                    case WorkspaceCommandKind::History: actionHistory.open = true; actionHistory.focusPending = true; break;
                    case WorkspaceCommandKind::AddSelection: {
                        BrowserRequests requests; requests.basket = CaptureLocation().selection; HandleBrowserRequests(requests, pluginBrowser.admission); break;
                    }
                    case WorkspaceCommandKind::PinA: comparison.a = selected; comparison.open = true; comparison.focusPending = true; break;
                    case WorkspaceCommandKind::CompareB: comparison.b = selected; comparison.open = true; comparison.focusPending = true; break;
                    default: break;
                    }
                }
                BrowserRequests comparisonRequests;
                DrawComparisonWindow(comparison, InspectorContext(catalog), comparisonRequests);
                if (!comparisonRequests.inspections.empty()) { inspectorTool = true; inspector.focusPending = true; }
                HandleBrowserRequests(comparisonRequests, pluginBrowser.admission);
            }
            if (basketOpen) {
                BasketRequests requests;
                DrawBasketWindow(basketOpen, basketView, basket, InspectorContext(catalog), ActionService::PendingCount(), requests);
                for (auto id : requests.inspect) { OpenInspector(id); inspectorTool = true; inspector.focusPending = true; }
                if (requests.history) { actionHistory.open = true; actionHistory.focusPending = true; }
                if (requests.save) {
                    auto document = *WorkspaceService::Read();
                    const bool unique = std::ranges::none_of(document.kits, [&](const auto& kit) { return FoldSearchText(kit.name) == FoldSearchText(requests.save->name); });
                    if (unique) document.kits.push_back(std::move(*requests.save));
                    basketView.saveFailed = !unique || !WorkspaceService::Commit(std::move(document));
                }
                if (requests.execute && !basketView.submitted) {
                    const auto review = ReviewItemKit(basket, *CatalogService::Read(), ActionService::Session(), ActionService::IsReady(), ActionService::PendingCount(), settings.componentSubstitution);
                    if (SameKitReview(*requests.execute, review)) {
                        auto actions = review.actions;
                        for (auto& action : actions) action.groupName = basketView.name[0] ? basketView.name.data() : L("Workspace", "sBasket", "Basket");
                        basketView.admission = ActionService::SubmitBatch(std::move(actions));
                        basketView.submitted = basketView.admission == ActionAdmission::Accepted;
                    } else basketView.admission = ActionAdmission::StaleSession;
                    basketView.reviewed.reset();
                }
            }
            if (DestinationFor(activeMainTab) == WorkspaceDestination::Explore && WorkspaceService::Writable()) {
                const auto location = CaptureLocation();
                const auto stored = WorkspaceService::Read();
                const auto match = [&](const auto& saved) { return saved.location.page == location.page && saved.location.category == location.category; };
                const auto found = std::ranges::find_if(stored->layouts, match);
                if (found == stored->layouts.end() || found->location.columns != location.columns ||
                    std::abs(found->location.sourceWidth - location.sourceWidth) > 0.5f || std::abs(found->location.inspectorWidth - location.inspectorWidth) > 0.5f ||
                    found->location.sourcesOpen != location.sourcesOpen || found->location.inspectorOpen != location.inspectorOpen ||
                    found->location.query.sortColumn != location.query.sortColumn || found->location.query.ascending != location.query.ascending) {
                    auto updated = *stored;
                    auto layout = WorkspaceLocation{};
                    layout.page = location.page; layout.category = location.category; layout.columns = location.columns;
                    layout.sourceWidth = location.sourceWidth; layout.inspectorWidth = location.inspectorWidth;
                    layout.sourcesOpen = location.sourcesOpen; layout.inspectorOpen = location.inspectorOpen;
                    layout.query.sortColumn = location.query.sortColumn; layout.query.ascending = location.query.ascending;
                    const auto item = std::ranges::find_if(updated.layouts, match);
                    if (item == updated.layouts.end()) updated.layouts.push_back({location.page + "/" + location.category, std::move(layout)});
                    else item->location = std::move(layout);
                    WorkspaceService::Commit(std::move(updated));
                }
            }

            for (const auto id : pendingPins) {
                if (pinnedInspectors.size() >= 3 || !catalog->Find(id)) continue;
                if (std::ranges::any_of(pinnedInspectors, [id](const auto& pin) { return pin.history.Current() && pin.history.Current()->formID == id; })) continue;
                RecordInspectorState pin;
                static std::uint64_t nextPinID{};
                pin.windowID = ++nextPinID;
                pin.history.Open(id, CaptureLocation());
                pin.focusPending = true;
                pinnedInspectors.push_back(std::move(pin));
            }
            pendingPins.clear();
            const auto drawInspectorTool = [&](RecordInspectorState& state, const std::string& id, bool restoreWorkspace) {
                ModalUtils::PrepareToolWindow(id.c_str(), {520, 650}, {300, 220}, state.focusPending);
                if (ImGui::Begin(id.c_str(), &state.open)) {
                    DrawInspector(state, InspectorContext(catalog), restoreWorkspace);
                    if (ModalUtils::EscapeClosesCurrentWindow()) state.open = false;
                }
                ImGui::End();
            };
            if (inspectorTool && inspector.open) drawInspectorTool(inspector, std::string(L("General", "sInspector", "Inspector")) + "###WorkspaceInspector", true);
            for (std::size_t index = 0; index < pinnedInspectors.size(); ++index)
                drawInspectorTool(pinnedInspectors[index], std::string(L("General", "sPinnedInspector", "Pinned Inspector")) + " " + std::to_string(index + 1) + "###PinnedInspector" + std::to_string(pinnedInspectors[index].windowID), false);
            std::erase_if(pinnedInspectors, [](const auto& pin) { return !pin.open; });
            if (diagnosticsOpen) {
                const auto diagnosticsTitle = std::string(L("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics")) + "###WorkspaceDiagnostics";
                ModalUtils::PrepareToolWindow(diagnosticsTitle.c_str(), {560, 600}, {300, 240}, diagnosticsFocus);
                if (ImGui::Begin(diagnosticsTitle.c_str(), &diagnosticsOpen)) {
                    if (ModalUtils::EscapeClosesCurrentWindow()) diagnosticsOpen = false;
                    if (ImGui::BeginCombo(L("General", "sPlugin", "Plugin"), pluginBrowser.diagnosticsPlugin.c_str())) {
                        for (const auto& plugin : catalog->plugins)
                            if (ImGui::Selectable(plugin.filename.c_str(), plugin.filename == pluginBrowser.diagnosticsPlugin)) pluginBrowser.diagnosticsPlugin = plugin.filename;
                        ImGui::EndCombo();
                    }
                    const auto plugin = std::ranges::find(catalog->plugins, pluginBrowser.diagnosticsPlugin, &PluginInfo::filename);
                    if (plugin != catalog->plugins.end()) PluginBrowserPanels::DrawDiagnostics(*plugin, InspectorContext(catalog));
                }
                ImGui::End();
            }

            // Submit tool windows independently of the selected page. The main
            // window cannot cover them when its workspace receives focus.
            const std::pair<const char*, AdvancedFilterEditorState*> editors[]{
                { "PluginBrowser", &pluginBrowser.filterEditor }, { "ItemBrowser", &itemBrowser.filterEditor },
                { "NPCBrowser", &npcBrowser.browser.filterEditor }, { "CellBrowser", &cellBrowser.filterEditor },
                { "ObjectBrowser", &objectBrowser.filterEditor }, { "SpellPerkBrowser", &spellPerkBrowser.filterEditor }
            };
            for (const auto& [id, editor] : editors) {
                if (RecordFiltersWidget::DrawEditor(L, id, { browserFilters.showNonPlayableRecords, browserFilters.showUnnamedRecords,
                        browserFilters.showDeletedRecords, browserFilters.advancedRecordFilters, browserFilters.hiddenPlugins }, *editor, catalog))
                    PersistListFilterSettings();
            }
            if (actionHistory.open) {
                const auto receipts = ActionService::Receipts();
                const auto position = DrawActionHistoryWindow(actionHistory, receipts, L, { settings.actionHistoryWindowX, settings.actionHistoryWindowY });
                if (actionHistory.inspect) {
                    OpenInspector(*actionHistory.inspect); inspectorTool = true; inspector.focusPending = true;
                    actionHistory.inspect.reset();
                }
                auto& updated = Config::GetMutable();
                if (std::fabs(updated.actionHistoryWindowX - position.x) > 0.5f || std::fabs(updated.actionHistoryWindowY - position.y) > 0.5f) {
                    updated.actionHistoryWindowX = position.x;
                    updated.actionHistoryWindowY = position.y;
                    settingsDirty = true;
                }
            }

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
            if (!GamepadInput::IsSteamKeyboardOpen() && ModalUtils::EscapeClosesCurrentWindow()) windowOpen = false;
        }
        ImGui::End();
        if (!windowOpen) {
            REX::DEBUG("{}", "Main window close button pressed");
            Hooks::SetMenuVisible(false);
        }
    }
}
