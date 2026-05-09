#include "GUI/Tabs/PluginBrowserPanels.h"

#include "Config/Config.h"

#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormDetailsView.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ESPExplorerAE::PluginBrowserPanels
{
    namespace
    {
        using namespace PluginBrowserHelpers;

        std::vector<std::uint32_t> previousVisibleRecordOrder{};

        const PluginInfo* FindPluginInfo(std::string_view pluginName, const std::vector<PluginInfo>& plugins)
        {
            const auto it = std::ranges::find_if(plugins, [pluginName](const PluginInfo& plugin) {
                return plugin.filename == pluginName;
            });

            return it != plugins.end() ? &(*it) : nullptr;
        }

        std::string CopyLabel(const char* label, std::size_t count)
        {
            return std::string(label) + " (" + std::to_string(count) + ")";
        }

        void DrawPluginDiagnosticsContent(const PluginInfo& plugin, PluginBrowserTabContext& context)
        {
            ImGui::Text("%s: %s", context.localize("General", "sPlugin", "Plugin"), plugin.filename.c_str());
            ImGui::Text("%s: %s", context.localize("General", "sType", "Type"), plugin.type.c_str());
            ImGui::Text("%s: %s", context.localize("PluginBrowser", "sPluginPrefix", "Form Prefix"), plugin.formIDPrefix.c_str());
            ImGui::Text("%s: %u", context.localize("PluginBrowser", "sRecordCount", "Record Count"), plugin.recordCount);
            ImGui::Text("%s: %u", context.localize("PluginBrowser", "sNewRecordCount", "New Records"), plugin.newRecordCount);
            ImGui::Text("%s: %u", context.localize("PluginBrowser", "sOverrideCount", "Overrides"), plugin.overrideCount);
            ImGui::Text("%s: %u", context.localize("PluginBrowser", "sOverriddenByOthersCount", "Overridden By Others"), plugin.overriddenByOthersCount);
            ImGui::Text("%s: %zu", context.localize("PluginBrowser", "sMasterCount", "Masters"), plugin.masters.size());

            if (plugin.missingMasters.empty()) {
                ImGui::Text("%s: %s", context.localize("PluginBrowser", "sMissingMasters", "Missing Masters"), context.localize("General", "sNo", "No"));
            } else {
                ImGui::TextColored(ImVec4(0.96f, 0.40f, 0.36f, 1.0f), "%s: %zu", context.localize("PluginBrowser", "sMissingMasters", "Missing Masters"), plugin.missingMasters.size());
                for (const auto& master : plugin.missingMasters) {
                    ImGui::BulletText("%s", master.c_str());
                }
            }

            if (!plugin.masters.empty() && ImGui::TreeNodeEx(context.localize("PluginBrowser", "sMasterList", "Master List"), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& master : plugin.masters) {
                    ImGui::BulletText("%s", master.c_str());
                }
                ImGui::TreePop();
            }
        }

        void DrawPluginDiagnosticsSection(const PluginInfo& plugin, PluginBrowserTabContext& context, bool collapsible)
        {
            if (collapsible) {
                ImGui::SetNextItemOpen(!context.collapseSelectedRecordDiagnostics, ImGuiCond_Always);
                const bool diagnosticsOpen = ImGui::CollapsingHeader(context.localize("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics"));
                context.collapseSelectedRecordDiagnostics = !diagnosticsOpen;
                if (diagnosticsOpen) {
                    DrawPluginDiagnosticsContent(plugin, context);
                }
                return;
            }

            ImGui::TextUnformatted(context.localize("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics"));
            ImGui::Separator();
            DrawPluginDiagnosticsContent(plugin, context);
        }

        void DrawRecordSelectable(
            const FormEntry& record,
            const char* idPrefix,
            const FormCache& cache,
            std::uint64_t dataVersion,
            PluginBrowserTabContext& context,
            const std::vector<std::uint32_t>& previousVisibleOrder,
            std::vector<std::uint32_t>& currentVisibleOrder)
        {
            currentVisibleOrder.push_back(record.formID);

            const auto* displayName = record.name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : record.name.c_str();
            const std::string formIDText = FormatUtils::FormID(record.formID);
            char recordLabel[512]{};
            std::snprintf(recordLabel, sizeof(recordLabel), "%s [%s]##%s%s", displayName, formIDText.c_str(), idPrefix, formIDText.c_str());
            const bool isSelected = context.selectedPluginTreeRecordFormIDs.contains(record.formID);
            if (ImGui::Selectable(recordLabel, isSelected)) {
                const bool shiftHeld = ImGui::GetIO().KeyShift;
                const bool ctrlHeld = ImGui::GetIO().KeyCtrl;

                if (shiftHeld && context.pluginTreeLastClickedFormID != 0 && !previousVisibleOrder.empty()) {
                    const auto anchorIt = std::find(previousVisibleOrder.begin(), previousVisibleOrder.end(), context.pluginTreeLastClickedFormID);
                    const auto currentIt = std::find(previousVisibleOrder.begin(), previousVisibleOrder.end(), record.formID);

                    if (anchorIt != previousVisibleOrder.end() && currentIt != previousVisibleOrder.end()) {
                        if (!ctrlHeld) {
                            context.selectedPluginTreeRecordFormIDs.clear();
                        }

                        auto beginIt = anchorIt;
                        auto endIt = currentIt;
                        if (beginIt > endIt) {
                            std::swap(beginIt, endIt);
                        }

                        for (auto it = beginIt; it != endIt + 1; ++it) {
                            context.selectedPluginTreeRecordFormIDs.insert(*it);
                        }

                        context.selectedPluginTreeRecordFormID = record.formID;
                        TrackRecentRecord(record.formID, context);
                    } else if (ctrlHeld) {
                        if (isSelected) {
                            context.selectedPluginTreeRecordFormIDs.erase(record.formID);
                        } else {
                            context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                            context.selectedPluginTreeRecordFormID = record.formID;
                            TrackRecentRecord(record.formID, context);
                        }
                    } else {
                        context.selectedPluginTreeRecordFormIDs.clear();
                        context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                        context.selectedPluginTreeRecordFormID = record.formID;
                        TrackRecentRecord(record.formID, context);
                    }
                } else if (ctrlHeld) {
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

                context.pluginTreeLastClickedFormID = record.formID;
                context.collapseSelectedRecordDiagnostics = true;
                EnsurePrimarySelectionValid(context);
            }

            if (ImGui::BeginPopupContextItem()) {
                if (!isSelected) {
                    context.selectedPluginTreeRecordFormIDs.clear();
                    context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                    context.selectedPluginTreeRecordFormID = record.formID;
                    context.pluginTreeLastClickedFormID = record.formID;
                    context.collapseSelectedRecordDiagnostics = true;
                    TrackRecentRecord(record.formID, context);
                }

                DrawRecordContextMenu(record, true, cache, dataVersion, context);

                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ContextMenu::CanGiveItem(record.category)) {
                context.openItemGrantPopup(record);
            }
        }

        template <class Range>
        void DrawRecordSection(const Range& records, const char* idPrefix, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context, const std::vector<std::uint32_t>& previousVisibleOrder, std::vector<std::uint32_t>& currentVisibleOrder)
        {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(records.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const auto* record = records[static_cast<std::size_t>(row)];
                    if (!record) {
                        continue;
                    }
                    DrawRecordSelectable(*record, idPrefix, cache, dataVersion, context, previousVisibleOrder, currentVisibleOrder);
                }
            }
        }

    }

    void DrawTreePane(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context, float leftWidth)
    {
        std::vector<std::uint32_t> currentVisibleRecordOrder{};
        currentVisibleRecordOrder.reserve((std::max)(std::size_t{ 256 }, context.pluginBrowserGlobalSearchResultsCache.size()));

        if (ImGui::BeginChild("PluginTreeLeft", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders)) {
            const std::string globalResultsHeader = std::string(context.localize("PluginBrowser", "sGlobalSearchResults", "Global Search Results")) + "##PluginGlobalSearchResults";
            if (context.pluginGlobalSearchMode && !context.pluginSearch.empty() && ImGui::TreeNodeEx(globalResultsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                DrawRecordSection(context.pluginBrowserGlobalSearchResultsCache, "GlobalResult", cache, dataVersion, context, previousVisibleRecordOrder, currentVisibleRecordOrder);
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

                    DrawRecordSelectable(*entry, "FavoriteRecord", cache, dataVersion, context, previousVisibleRecordOrder, currentVisibleRecordOrder);
                }
                ImGui::TreePop();
            }

            const std::string recentRecordsHeader = std::string(context.localize("PluginBrowser", "sRecentRecords", "Recent Records")) + "##PluginRecentRecords";
            if (ImGui::TreeNodeEx(recentRecordsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                const bool hasRecentRecords = !context.recentPluginRecordFormIDs.empty();
                const ImVec4 buttonColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);
                const ImVec4 hoveredColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
                const ImVec4 activeColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(buttonColor.x, buttonColor.y, buttonColor.z, buttonColor.w * 0.55f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(hoveredColor.x, hoveredColor.y, hoveredColor.z, hoveredColor.w * 0.75f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(activeColor.x, activeColor.y, activeColor.z, activeColor.w * 0.85f));
                if (!hasRecentRecords) {
                    ImGui::BeginDisabled(true);
                }
                if (ImGui::SmallButton(context.localize("PluginBrowser", "sClearRecentRecords", "Clear Recent Records"))) {
                    context.recentPluginRecordFormIDs.clear();
                }
                if (!hasRecentRecords) {
                    ImGui::EndDisabled();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();

                const std::size_t recentRecordsLimit = static_cast<std::size_t>((std::clamp)(Config::Get().recentRecordsLimit, 5, 100));
                while (context.recentPluginRecordFormIDs.size() > recentRecordsLimit) {
                    context.recentPluginRecordFormIDs.pop_back();
                }

                std::size_t displayedRecentRecords = 0;
                for (const auto recentFormID : context.recentPluginRecordFormIDs) {
                    if (displayedRecentRecords >= recentRecordsLimit) {
                        break;
                    }

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

                    DrawRecordSelectable(*recentEntry, "RecentRecord", cache, dataVersion, context, previousVisibleRecordOrder, currentVisibleRecordOrder);
                    ++displayedRecentRecords;
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

                const std::string pluginLabel = BuildPluginDisplayName(pluginName, plugins) + " (" + std::to_string(totalRecords) + ")";
                const bool pluginNodeOpen = ImGui::TreeNodeEx(pluginLabel.c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    context.selectedPluginDiagnostics = pluginName;
                    context.selectedPluginTreeRecordFormID = 0;
                    context.selectedPluginTreeRecordFormIDs.clear();
                    context.pluginTreeLastClickedFormID = 0;
                    context.collapseSelectedRecordDiagnostics = false;
                }
                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    context.selectedPluginDiagnostics = pluginName;
                    context.selectedPluginFilter = pluginName;
                    context.selectedPluginTreeRecordFormID = 0;
                    context.selectedPluginTreeRecordFormIDs.clear();
                    context.pluginTreeLastClickedFormID = 0;
                    context.collapseSelectedRecordDiagnostics = false;
                }

                if (pluginNodeOpen) {
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
                            DrawRecordSection(categoryIt->second, "TreeRecord", cache, dataVersion, context, previousVisibleRecordOrder, currentVisibleRecordOrder);
                            ImGui::TreePop();
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        previousVisibleRecordOrder = std::move(currentVisibleRecordOrder);
    }

    void DrawDetailsPane(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
        if (ImGui::BeginChild("PluginTreeDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            EnsurePrimarySelectionValid(context);
            const FormEntry* selectedRecord = context.selectedPluginTreeRecordFormID != 0 ? FindRecordByFormID(cache, context.selectedPluginTreeRecordFormID, dataVersion) : nullptr;
            const auto selectedEntries = CollectSelectedEntries(cache, dataVersion, context);
            const auto selectedGiveableEntries = CollectSelectedGiveableEntries(cache, dataVersion, context);
            const bool hasMultipleSelection = context.selectedPluginTreeRecordFormIDs.size() > 1;
            const auto diagnosticPluginName = selectedRecord ? selectedRecord->sourcePlugin : (!context.selectedPluginDiagnostics.empty() ? context.selectedPluginDiagnostics : context.selectedPluginFilter);
            const PluginInfo* diagnosticPlugin = diagnosticPluginName.empty() ? nullptr : FindPluginInfo(diagnosticPluginName, plugins);

            if (!selectedRecord) {
                if (diagnosticPlugin) {
                    DrawPluginDiagnosticsSection(*diagnosticPlugin, context, false);
                } else {
                    ImGui::TextUnformatted(context.localize("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
                }
            } else {
                const float totalAvail = ImGui::GetContentRegionAvail().y;
                const float minDetailsHeight = ImGui::GetFrameHeightWithSpacing() * 6.0f;
                const float detailsHeight = (std::max)(minDetailsHeight, totalAvail * 0.55f);
                if (ImGui::BeginChild("PluginDetailsInfo", ImVec2(0.0f, detailsHeight), false)) {
                    FormDetailsViewContext detailsContext{
                        .localize = context.localize,
                        .showAdvancedDetailsView = context.showAdvancedDetailsView
                    };
                    FormDetailsView::Draw(*selectedRecord, detailsContext);
                }
                ImGui::EndChild();

                ImGui::Separator();
                if (ImGui::BeginChild("PluginDetailsActions", ImVec2(0.0f, 0.0f), false)) {
                    if (diagnosticPlugin) {
                        DrawPluginDiagnosticsSection(*diagnosticPlugin, context, true);
                        ImGui::Separator();
                    }

                    const bool canGive = ContextMenu::CanGiveItem(selectedRecord->category);
                    const bool canSpawn = ContextMenu::CanSpawn(selectedRecord->category);
                    const bool canTeleport = ContextMenu::CanTeleport(selectedRecord->category);
                    const bool isQuest = ContextMenu::IsQuest(selectedRecord->category);
                    const bool isPerk = ContextMenu::IsPerk(selectedRecord->category);
                    const bool isSpellLike = ContextMenu::IsSpellLike(selectedRecord->category);
                    const bool isWeather = ContextMenu::IsWeather(selectedRecord->category);
                    const bool isSound = ContextMenu::IsSound(selectedRecord->category);
                    const bool isGlobal = ContextMenu::IsGlobal(selectedRecord->category);
                    const bool isOutfit = ContextMenu::IsOutfit(selectedRecord->category);
                    const bool isConstructible = ContextMenu::IsConstructible(selectedRecord->category);
                    const bool isEquippable = ContextMenu::IsEquippable(selectedRecord->category);
                    const bool gameplayActionsAllowed = FormActions::AreGameplayActionsAllowed();
                    const char* disabledTooltip = context.localize("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");

                    bool firstBtn = true;
                    ImGuiWidgetUtils::FixedGridButtonRow buttonRow{};
                    const auto drawActionButton = [&](const char* label) {
                        return ImGuiWidgetUtils::DrawFixedGridButton(label, firstBtn, buttonRow);
                    };

                    if (hasMultipleSelection && !selectedEntries.empty()) {
                        if (drawActionButton(CopyLabel(context.localize("General", "sCopyName", "Copy Name"), selectedEntries.size()).c_str())) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                values.push_back(selectedEntry.name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                            }
                            const auto text = FormatUtils::MultiCopyList(values, Config::Get().multiCopyFormat);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (drawActionButton(CopyLabel(context.localize("General", "sCopyFormID", "Copy FormID"), selectedEntries.size()).c_str())) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                values.push_back(FormatUtils::FormID(selectedEntry.formID));
                            }
                            const auto text = FormatUtils::MultiCopyList(values, Config::Get().multiCopyFormat);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (drawActionButton((std::string(context.localize("General", "sAddFavorite", "Add Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                            for (const auto& selectedEntry : selectedEntries) {
                                context.favoriteForms.insert(selectedEntry.formID);
                            }
                        }

                        if (drawActionButton((std::string(context.localize("General", "sRemoveFavorite", "Remove Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                            for (const auto& selectedEntry : selectedEntries) {
                                context.favoriteForms.erase(selectedEntry.formID);
                            }
                        }
                    } else {
                        if (drawActionButton(context.localize("General", "sCopyFormID", "Copy FormID"))) {
                            FormActions::CopyFormID(selectedRecord->formID);
                        }

                        if (drawActionButton(context.localize("General", "sCopyRecordSource", "Copy Record Source"))) {
                            ImGui::SetClipboardText(selectedRecord->sourcePlugin.c_str());
                        }

                        if (drawActionButton(context.localize("General", "sCopyName", "Copy Name"))) {
                            ImGui::SetClipboardText(selectedRecord->name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedRecord->name.c_str());
                        }

                        if (canGive || isEquippable) {
                            if (!gameplayActionsAllowed) {
                                ImGui::BeginDisabled(true);
                            }
                            if (canGive) {
                                if (drawActionButton(context.localize("Items", "sGiveItem", "Give Item"))) {
                                    context.openItemGrantPopup(*selectedRecord);
                                }
                                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                            }

                            if (isEquippable) {
                                if (drawActionButton(context.localize("General", "sEquipItem", "Equip Item"))) {
                                    EquipRecordWithConfiguredAmmo(*selectedRecord, context.equipWeaponAmmoCount);
                                }
                                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                            }
                            if (!gameplayActionsAllowed) {
                                ImGui::EndDisabled();
                            }
                        }

                        const bool isFavorite = context.favoriteForms.contains(selectedRecord->formID);
                        if (drawActionButton(isFavorite ? context.localize("General", "sRemoveFavorite", "Remove Favorite") : context.localize("General", "sAddFavorite", "Add Favorite"))) {
                            if (isFavorite) {
                                context.favoriteForms.erase(selectedRecord->formID);
                            } else {
                                context.favoriteForms.insert(selectedRecord->formID);
                            }
                        }
                    }

                    if (!gameplayActionsAllowed) {
                        ImGui::BeginDisabled(true);
                    }
                    if (hasMultipleSelection && !selectedGiveableEntries.empty()) {
                        std::string giveSelectedLabel = std::string(context.localize("Items", "sGiveItem", "Give Item")) + " (" + std::to_string(selectedGiveableEntries.size()) + ")";
                        if (drawActionButton(giveSelectedLabel.c_str())) {
                            context.openItemGrantPopupMultiple(selectedGiveableEntries);
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (canSpawn || canGive) {
                        const char* spawnLabel = context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player");

                        static int detailSpawnQuantity = 1;
                        if (drawActionButton(spawnLabel)) {
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
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                        const auto& style = ImGui::GetStyle();
                        const float quantityWidth = 140.0f;
                        if (ImGui::GetContentRegionAvail().x >= (style.ItemSpacing.x + quantityWidth)) {
                            ImGui::SameLine();
                        }
                        ImGui::SetNextItemWidth(quantityWidth);
                        ImGui::InputInt("##DetailSpawnQty", &detailSpawnQuantity, 1, 10);
                        if (detailSpawnQuantity < 1) {
                            detailSpawnQuantity = 1;
                        }

                        firstBtn = true;
                        buttonRow = {};
                    }

                    if (isQuest) {
                        if (drawActionButton(context.localize("General", "sStartQuest", "Start Quest"))) {
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
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                        if (drawActionButton(context.localize("General", "sCompleteQuest", "Complete Quest"))) {
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
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isPerk) {
                        if (drawActionButton(context.localize("General", "sAddPerk", "Add Perk"))) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddPerk", "Add selected perk to player?"),
                                [formID]() {
                                    FormActions::AddPerkToPlayer(formID);
                                });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                        if (drawActionButton(context.localize("General", "sRemovePerk", "Remove Perk"))) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmRemovePerk", "Remove selected perk from player?"),
                                [formID]() {
                                    FormActions::RemovePerkFromPlayer(formID);
                                });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isSpellLike) {
                        if (drawActionButton(context.localize("General", "sAddSpellEffect", "Add Spell/Effect"))) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddSpellEffect", "Add selected spell/effect to player?"),
                                [formID]() {
                                    FormActions::AddSpellToPlayer(formID);
                                });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                        if (drawActionButton(context.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"))) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmRemoveSpellEffect", "Remove selected spell/effect from player?"),
                                [formID]() {
                                    FormActions::RemoveSpellFromPlayer(formID);
                                });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isWeather) {
                        if (drawActionButton(context.localize("General", "sSetWeather", "Set Weather"))) {
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
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isSound) {
                        if (drawActionButton(context.localize("General", "sPlaySound", "Play Sound"))) {
                            if (const char* editorID = ContextMenu::TryGetEditorID(selectedRecord->formID)) {
                                std::string command = std::string("playsound ") + editorID;
                                FormActions::ExecuteConsoleCommand(command);
                            }
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isGlobal) {
                        if (drawActionButton(context.localize("General", "sSetGlobal", "Set Global"))) {
                            context.openGlobalValuePopup(selectedRecord->formID);
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isOutfit) {
                        if (drawActionButton(context.localize("General", "sAddOutfitItems", "Add Outfit Items"))) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddOutfitItems", "Add all items from selected outfit to player?"),
                                [formID]() {
                                    FormActions::AddOutfitItemsToPlayer(formID);
                                });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (isConstructible) {
                        if (drawActionButton(context.localize("General", "sAddCraftedItem", "Add Crafted Item"))) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddCraftedItem", "Add crafted output of selected recipe to player?"),
                                [formID]() {
                                    FormActions::AddConstructedItemToPlayer(formID);
                                });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (canTeleport) {
                        auto* teleportForm = RE::TESForm::GetFormByID(selectedRecord->formID);
                        const char* editorID = teleportForm ? teleportForm->GetFormEditorID() : nullptr;
                        const bool canUseCoc = editorID && editorID[0] != '\0';

                        if (canUseCoc) {
                            if (drawActionButton(context.localize("General", "sTeleportCOC", "Teleport (COC)"))) {
                                const auto editorIDCopy = std::string(editorID);
                                context.requestActionConfirmation(
                                    context.localize("General", "sConfirmTeleportTitle", "Confirm Teleport"),
                                    context.localize("General", "sConfirmTeleport", "Teleport to selected destination?"),
                                    [editorIDCopy]() {
                                        std::string command = std::string("coc ") + editorIDCopy;
                                        FormActions::ExecuteConsoleCommand(command);
                                    });
                            }
                            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                        } else {
                            ImGui::BeginDisabled(true);
                            drawActionButton(context.localize("General", "sTeleportCOC", "Teleport (COC)"));
                            ImGui::EndDisabled();
                        }
                    }
                    if (!gameplayActionsAllowed) {
                        ImGui::EndDisabled();
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::EndChild();
    }
}
