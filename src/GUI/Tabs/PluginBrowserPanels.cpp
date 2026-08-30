#include "GUI/Tabs/PluginBrowserPanels.h"



#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Widgets/BrowserWidgets.h"
#include "Core/RecordActions.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/FormDetailsView.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"

#include <imgui.h>

#include <algorithm>
#include <ranges>

namespace ESPExplorerAE::PluginBrowserPanels
{
    namespace
    {
        using namespace PluginBrowserHelpers;

        struct TreeFrame
        {
            struct Click { TreeRow row; bool control; bool shift; };
            struct Section { std::string key; std::span<const RecordIndex> records; };
            std::vector<Section> sections;
            std::vector<RecordIndex> favorites;
            std::vector<RecordIndex> recent;
            std::optional<Click> click;
        };

        const PluginInfo* FindPluginInfo(std::string_view pluginName, const std::vector<PluginInfo>& plugins)
        {
            const auto it = std::ranges::find_if(plugins, [pluginName](const PluginInfo& plugin) {
                return plugin.filename == pluginName;
            });

            return it != plugins.end() ? &(*it) : nullptr;
        }

        std::string CopyLabel(const char* label, std::size_t count, const char* id)
        {
            return std::string(label) + " (" + std::to_string(count) + ")###" + id;
        }

        void DrawPluginDiagnosticsContent(const PluginInfo& plugin, Context& context)
        {
            ImGui::Text("%s: %s", context.view.records.localize("General", "sPlugin", "Plugin"), plugin.filename.c_str());
            ImGui::Text("%s: %s", context.view.records.localize("General", "sType", "Type"), plugin.type.c_str());
            ImGui::Text("%s: %s", context.view.records.localize("PluginBrowser", "sPluginPrefix", "Form Prefix"), plugin.formIDPrefix.c_str());
            ImGui::Text("%s: %u", context.view.records.localize("PluginBrowser", "sRuntimeSourceRecords", "Runtime source records"), plugin.runtimeSourceRecords);
            ImGui::Text("%s: %u", context.view.records.localize("PluginBrowser", "sRuntimeFormIDOrigins", "Runtime FormID origins"), plugin.runtimeOriginRecords);
            const auto drawOptionalCount = [&](const char* key, const char* fallback, std::optional<std::uint32_t> count) {
                const auto* label = context.view.records.localize("PluginBrowser", key, fallback);
                if (count) ImGui::Text("%s: %u", label, *count);
                else ImGui::Text("%s: %s", label, context.view.records.localize("PluginBrowser", "sDiagnosticUnavailable", "Unavailable"));
            };
            drawOptionalCount("sOverrideCount", "Overrides", plugin.overrideCount);
            drawOptionalCount("sOverriddenByOthersCount", "Overridden By Others", plugin.overriddenByOthersCount);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", context.view.records.localize("PluginBrowser", "sDiagnosticsScope",
                "Runtime-known forms only. Source is the reported file; origin is the runtime FormID owner. Override history and complete file totals are unavailable."));
            ImGui::Text("%s: %zu", context.view.records.localize("PluginBrowser", "sMasterCount", "Masters"), plugin.masters.size());

            if (plugin.missingMasters.empty()) {
                ImGui::Text("%s: %s", context.view.records.localize("PluginBrowser", "sMissingMasters", "Missing Masters"), context.view.records.localize("General", "sNo", "No"));
            } else {
                ImGui::TextColored(ImVec4(0.96f, 0.40f, 0.36f, 1.0f), "%s: %zu", context.view.records.localize("PluginBrowser", "sMissingMasters", "Missing Masters"), plugin.missingMasters.size());
                for (const auto& master : plugin.missingMasters) {
                    ImGui::BulletText("%s", master.c_str());
                }
            }

            if (!plugin.masters.empty() && ImGui::TreeNodeEx(context.view.records.localize("PluginBrowser", "sMasterList", "Master List"), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& master : plugin.masters) {
                    ImGui::BulletText("%s", master.c_str());
                }
                ImGui::TreePop();
            }
        }

        void DrawPluginDiagnosticsSection(const PluginInfo& plugin, Context& context, bool collapsible)
        {
            if (collapsible) {
                ImGui::SetNextItemOpen(!context.state.collapseDiagnostics, ImGuiCond_Always);
                const bool diagnosticsOpen = ImGui::CollapsingHeader(context.view.records.localize("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics"));
                context.state.collapseDiagnostics = !diagnosticsOpen;
                if (diagnosticsOpen) {
                    DrawPluginDiagnosticsContent(plugin, context);
                }
                return;
            }

            ImGui::TextUnformatted(context.view.records.localize("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics"));
            ImGui::Separator();
            DrawPluginDiagnosticsContent(plugin, context);
        }

        void DrawRecordSelectable(const FormEntry& record, const char* idPrefix,
            const CatalogSnapshot& cache, Context& context, TreeFrame& frame)
        {
            const TreeRow row{ idPrefix, record.formID };
            const auto* displayName = record.name.empty() ? context.view.records.localize("General", "sUnnamed", "<Unnamed>") : record.name.c_str();
            const auto formIDText = FormatUtils::FormID(record.formID);
            const auto recordLabel = std::string(displayName) + " [" + formIDText + "]###" + idPrefix + formIDText;
            const bool isSelected = context.state.selection.records.selected.contains(record.formID);
            if (ImGui::Selectable(recordLabel.c_str(), isSelected)) {
                // Apply after all expanded sections supply their complete order.
                frame.click = TreeFrame::Click{ row, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift };
            }
            if (ImGui::BeginPopupContextItem()) {
                if (!context.state.selection.records.selected.contains(record.formID)) {
                    context.state.selection.Single(row);
                    context.state.collapseDiagnostics = true;
                    TrackRecentRecord(record.formID, context);
                }
                frame.click.reset();
                DrawRecordContextMenu(record, true, cache, context);
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && SupportsRecordAction(record.category, ActionKind::Give) && !record.isDeleted && context.view.records.gameplayReady) {
                context.requests.records.grants.push_back({ context.view.records.session, { record.formID } });
            }
        }

        void DrawRecordSection(const std::vector<RecordIndex>& records, const char* idPrefix,
            const CatalogSnapshot& cache, Context& context, TreeFrame& frame)
        {
            // The clipper controls rendering only. Range selection sees every
            // logical row, including separate occurrences in favorites/recents.
            frame.sections.push_back({ idPrefix, records });
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(records.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    DrawRecordSelectable(cache.records[records[static_cast<std::size_t>(row)]], idPrefix, cache, context, frame);
                }
            }
        }

    }

    void DrawTreePane(const std::vector<PluginInfo>& plugins, const CatalogSnapshot& cache, Context& context, float leftWidth)
    {
        TreeFrame frame;
        const auto& result = context.state.query.Result();

        if (ImGui::BeginChild("PluginTreeLeft", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders)) {
            const std::string globalResultsHeader = std::string(context.view.records.localize("PluginBrowser", "sGlobalSearchResults", "Global Search Results")) + "###PluginGlobalSearchResults";
            if (context.state.globalSearch && !context.state.search.empty() && ImGui::TreeNodeEx(globalResultsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                DrawRecordSection(result.records.order, "GlobalResult", cache, context, frame);
                ImGui::TreePop();
            }

            const std::string favoritesHeader = std::string(context.view.records.localize("General", "sFavorites", "Favorites")) + "###PluginFavorites";
            if (ImGui::TreeNodeEx(favoritesHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                if (context.view.favoriteReviewCount) ImGui::TextWrapped("%s: %zu", context.view.records.localize("Favorites", "sReviewInSettings", "Favorites to review in Settings"), context.view.favoriteReviewCount);
                for (const auto id : context.view.records.favorites) if (result.eligibleIDs.contains(id)) frame.favorites.push_back(cache.byID.at(id));
                std::ranges::sort(frame.favorites);
                DrawRecordSection(frame.favorites, "FavoriteRecord", cache, context, frame);
                ImGui::TreePop();
            }

            const std::string recentRecordsHeader = std::string(context.view.records.localize("PluginBrowser", "sRecentRecords", "Recent Records")) + "###PluginRecentRecords";
            if (ImGui::TreeNodeEx(recentRecordsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                const bool hasRecentRecords = !context.state.recent.empty();
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
                if (ImGui::SmallButton(context.view.records.localize("PluginBrowser", "sClearRecentRecords", "Clear Recent Records"))) {
                    context.state.recent.clear();
                }
                if (!hasRecentRecords) {
                    ImGui::EndDisabled();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();

                const std::size_t recentRecordsLimit = context.view.recentLimit;
                while (context.state.recent.size() > recentRecordsLimit) {
                    context.state.recent.pop_back();
                }

                for (const auto id : context.state.recent) {
                    if (result.eligibleIDs.contains(id)) frame.recent.push_back(cache.byID.at(id));
                }
                DrawRecordSection(frame.recent, "RecentRecord", cache, context, frame);
                ImGui::TreePop();
            }

            ImGui::Separator();

            for (const auto& pluginName : result.plugins) {
                auto pluginIt = result.groups.find(pluginName);
                if (pluginIt == result.groups.end()) {
                    continue;
                }

                std::size_t totalRecords = 0;
                for (const auto& [_, list] : pluginIt->second) {
                    totalRecords += list.size();
                }

                const std::string pluginLabel = (pluginName.empty() ? std::string(context.view.records.localize("General", "sUnknown", "<Unknown>")) : BuildPluginDisplayName(pluginName, plugins)) + " (" + std::to_string(totalRecords) + ")###Plugin:" + pluginName;
                const bool pluginNodeOpen = ImGui::TreeNodeEx(pluginLabel.c_str(), ImGuiTreeNodeFlags_OpenOnArrow);
                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    context.state.diagnosticsPlugin = pluginName;
                    context.state.selection.Clear();
                    context.state.collapseDiagnostics = false;
                }
                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    context.state.diagnosticsPlugin = pluginName;
                    context.requests.pluginFilter = pluginName;
                    context.state.selection.Clear();
                    context.state.collapseDiagnostics = false;
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
                        categoryLabel += "###Category:" + category;
                        ImGui::PushStyleColor(ImGuiCol_Text, CategoryColor(category));
                        if (ImGui::TreeNode(categoryLabel.c_str())) {
                            DrawRecordSection(categoryIt->second, ("TreeRecord/" + pluginName + "/" + category).c_str(), cache, context, frame);
                            ImGui::TreePop();
                        }
                        ImGui::PopStyleColor();
                    }

                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        if (frame.click) {
            std::vector<TreeRow> order;
            for (const auto& section : frame.sections) {
                for (const auto index : section.records) order.push_back({ section.key, cache.records[index].formID });
            }
            context.state.selection.Click(order, frame.click->row, frame.click->control, frame.click->shift);
            context.state.collapseDiagnostics = true;
            TrackRecentRecord(frame.click->row.formID, context);
            EnsurePrimarySelectionValid(context);
        }
    }

    void DrawDetailsPane(const std::vector<PluginInfo>& plugins, const CatalogSnapshot& cache, Context& context)
    {
        if (ImGui::BeginChild("PluginTreeDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            EnsurePrimarySelectionValid(context);
            const FormEntry* selectedRecord = context.state.selection.records.active != 0 ? FindRecordByFormID(cache, context.state.selection.records.active) : nullptr;
            const auto selectedIndices = CollectSelectedEntries(cache, context);
            const auto selectedEntries = selectedIndices | std::views::transform([&](RecordIndex index) -> const FormEntry& { return cache.records[index]; });
            const auto selectedGiveableEntries = CollectSelectedGiveableEntries(cache, context);
            const bool hasMultipleSelection = context.state.selection.records.selected.size() > 1;
            const auto diagnosticPluginName = selectedRecord ? selectedRecord->sourcePlugin : (!context.state.diagnosticsPlugin.empty() ? context.state.diagnosticsPlugin : context.view.records.pluginFilter);
            const PluginInfo* diagnosticPlugin = diagnosticPluginName.empty() ? nullptr : FindPluginInfo(diagnosticPluginName, plugins);

            if (!selectedRecord) {
                if (diagnosticPlugin) {
                    DrawPluginDiagnosticsSection(*diagnosticPlugin, context, false);
                } else {
                    ImGui::TextUnformatted(context.view.records.localize("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
                }
            } else {
                const float totalAvail = ImGui::GetContentRegionAvail().y;
                const float minDetailsHeight = ImGui::GetFrameHeightWithSpacing() * 6.0f;
                const float detailsHeight = (std::max)(minDetailsHeight, totalAvail * 0.55f);
                if (ImGui::BeginChild("PluginDetailsInfo", ImVec2(0.0f, detailsHeight), false)) {
                    context.requests.details = DetailKey{ selectedRecord->formID, context.view.records.session, cache.generation, context.view.advancedDetails };
                    FormDetailsViewContext detailsContext{
                        .localize = context.view.records.localize,
                        .showAdvancedDetailsView = context.view.advancedDetails,
                        .catalog = &cache,
                        .details = context.view.details && context.view.details->key == *context.requests.details ? context.view.details : nullptr
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

                    const bool canGive = SupportsRecordAction(selectedRecord->category, ActionKind::Give);
                    const bool canSpawn = SupportsRecordAction(selectedRecord->category, ActionKind::Spawn);
                    const bool isEquippable = SupportsRecordAction(selectedRecord->category, ActionKind::Equip);
                    const bool gameplayActionsAllowed = context.view.records.gameplayReady && !selectedRecord->isDeleted;
                    const char* disabledTooltip = context.view.records.localize("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");

                    bool firstBtn = true;
                    ImGuiWidgetUtils::FixedGridButtonRow buttonRow{};
                    const auto drawActionButton = [&](const char* label) {
                        return ImGuiWidgetUtils::DrawFixedGridButton(label, firstBtn, buttonRow);
                    };

                    if (hasMultipleSelection && !selectedEntries.empty()) {
                        if (drawActionButton(CopyLabel(context.view.records.localize("General", "sCopyName", "Copy Name"), selectedEntries.size(), "sCopyName").c_str())) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                values.push_back(selectedEntry.name.empty() ? context.view.records.localize("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                            }
                            const auto text = FormatUtils::MultiCopyList(values, context.view.copyFormat);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (drawActionButton(CopyLabel(context.view.records.localize("General", "sCopyFormID", "Copy FormID"), selectedEntries.size(), "sCopyFormID").c_str())) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                values.push_back(FormatUtils::FormID(selectedEntry.formID));
                            }
                            const auto text = FormatUtils::MultiCopyList(values, context.view.copyFormat);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (drawActionButton(CopyLabel(context.view.records.localize("General", "sAddFavorite", "Add Favorite"), selectedEntries.size(), "sAddFavorite").c_str())) {
                            for (const auto& selectedEntry : selectedEntries) {
                                context.view.records.favorites.insert(selectedEntry.formID);
                            }
                        }

                        if (drawActionButton(CopyLabel(context.view.records.localize("General", "sRemoveFavorite", "Remove Favorite"), selectedEntries.size(), "sRemoveFavorite").c_str())) {
                            for (const auto& selectedEntry : selectedEntries) {
                                context.view.records.favorites.erase(selectedEntry.formID);
                            }
                        }
                    } else {
                        if (drawActionButton(context.view.records.localize("General", "sCopyFormID", "Copy FormID"))) {
                            ImGui::SetClipboardText(FormatUtils::FormID(selectedRecord->formID).c_str());
                        }

                        if (drawActionButton(context.view.records.localize("General", "sCopyRecordSource", "Copy Record Source"))) {
                            ImGui::SetClipboardText(selectedRecord->sourcePlugin.c_str());
                        }

                        if (drawActionButton(context.view.records.localize("General", "sCopyName", "Copy Name"))) {
                            ImGui::SetClipboardText(selectedRecord->name.empty() ? context.view.records.localize("General", "sUnnamed", "<Unnamed>") : selectedRecord->name.c_str());
                        }

                        if (canGive || isEquippable) {
                            if (!gameplayActionsAllowed) {
                                ImGui::BeginDisabled(true);
                            }
                            if (canGive) {
                                if (drawActionButton(context.view.records.localize("Items", "sGiveItem", "Give Item"))) {
                                    context.requests.records.grants.push_back({ context.view.records.session, { selectedRecord->formID } });
                                }
                                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                            }

                            if (isEquippable) {
                                if (drawActionButton(context.view.records.localize("General", "sEquipItem", "Equip Item"))) {
                                    BrowserWidgets::Emit(context.requests.records, context.view.records, *selectedRecord, ActionKind::Equip);
                                }
                                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                            }
                            if (!gameplayActionsAllowed) {
                                ImGui::EndDisabled();
                            }
                        }

                        const bool isFavorite = context.view.records.favorites.contains(selectedRecord->formID);
                        if (drawActionButton(isFavorite ? context.view.records.localize("General", "sRemoveFavorite", "Remove Favorite") : context.view.records.localize("General", "sAddFavorite", "Add Favorite"))) {
                            if (isFavorite) {
                                context.view.records.favorites.erase(selectedRecord->formID);
                            } else {
                                context.view.records.favorites.insert(selectedRecord->formID);
                            }
                        }
                    }

                    if (!gameplayActionsAllowed) {
                        ImGui::BeginDisabled(true);
                    }
                    if (hasMultipleSelection && !selectedGiveableEntries.empty()) {
                        std::string giveSelectedLabel = std::string(context.view.records.localize("Items", "sGiveItem", "Give Item")) + " (" + std::to_string(selectedGiveableEntries.size()) + ")###GiveSelection";
                        if (drawActionButton(giveSelectedLabel.c_str())) {
                            RequestGrant(selectedGiveableEntries, context);
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                    }

                    if (canSpawn || canGive) {
                        const char* spawnLabel = context.view.records.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player");

                        int& detailSpawnQuantity = context.state.spawnQuantity;
                        if (drawActionButton(spawnLabel)) {
                            BrowserWidgets::Emit(context.requests.records, context.view.records, *selectedRecord,
                                ActionKind::Spawn, true, static_cast<std::uint32_t>(detailSpawnQuantity));
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                        const auto& style = ImGui::GetStyle();
                        const float quantityWidth = 140.0f;
                        if (ImGui::GetContentRegionAvail().x >= (style.ItemSpacing.x + quantityWidth)) {
                            ImGui::SameLine();
                        }
                        ImGui::SetNextItemWidth(quantityWidth);
                        ImGui::InputInt("##DetailSpawnQty", &detailSpawnQuantity, 1, 10);
                        detailSpawnQuantity = (std::clamp)(detailSpawnQuantity, 1, static_cast<int>(ActionQueue::MaxQuantity));

                        firstBtn = true;
                        buttonRow = {};
                    }

                    const auto drawRecordAction = [&](ActionKind kind, const char* key, const char* fallback, bool confirm = true) {
                        if (!SupportsRecordAction(selectedRecord->category, kind)) return;
                        const auto request = PrepareRecordAction(kind, *selectedRecord, context.view.records.session, gameplayActionsAllowed);
                        ImGui::BeginDisabled(!request);
                        const auto label = std::string(context.view.records.localize("General", key, fallback)) + "###" + key;
                        if (drawActionButton(label.c_str())) BrowserWidgets::Emit(context.requests.records, context.view.records, *selectedRecord, kind, confirm);
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                        ImGui::EndDisabled();
                    };
                    drawRecordAction(ActionKind::StartQuest, "sStartQuest", "Start Quest");
                    drawRecordAction(ActionKind::CompleteQuest, "sCompleteQuest", "Complete Quest");
                    drawRecordAction(ActionKind::AddPerk, "sAddPerk", "Add Perk");
                    drawRecordAction(ActionKind::RemovePerk, "sRemovePerk", "Remove Perk");
                    drawRecordAction(ActionKind::AddSpell, "sAddSpellEffect", "Add Spell/Effect");
                    drawRecordAction(ActionKind::RemoveSpell, "sRemoveSpellEffect", "Remove Spell/Effect");
                    drawRecordAction(ActionKind::SetWeather, "sSetWeather", "Set Weather");
                    drawRecordAction(ActionKind::PlaySound, "sPlaySound", "Play Sound", false);
                    if (SupportsRecordAction(selectedRecord->category, ActionKind::SetGlobal)) {
                        ImGui::BeginDisabled(selectedRecord->editorID.empty());
                        if (drawActionButton(context.view.records.localize("General", "sSetGlobal", "Set Global"))) {
                            context.requests.records.globalValues.push_back({ context.view.records.session, selectedRecord->formID });
                        }
                        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                        ImGui::EndDisabled();
                    }
                    drawRecordAction(ActionKind::Outfit, "sAddOutfitItems", "Add Outfit Items");
                    drawRecordAction(ActionKind::ConstructedItem, "sAddCraftedItem", "Add Crafted Item");
                    drawRecordAction(ActionKind::Teleport, "sTeleportCOC", "Teleport (COC)");
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
