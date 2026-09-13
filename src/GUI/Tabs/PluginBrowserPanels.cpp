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

        void DrawPluginDiagnosticsContent(const PluginInfo& plugin, const BrowserView& view)
        {
            ImGui::Text("%s: %s", view.localize("General", "sPlugin", "Plugin"), plugin.filename.c_str());
            ImGui::Text("%s: %s", view.localize("General", "sType", "Type"), plugin.type.c_str());
            ImGui::Text("%s: %s", view.localize("PluginBrowser", "sPluginPrefix", "Form Prefix"), plugin.formIDPrefix.c_str());
            ImGui::Text("%s: %u", view.localize("PluginBrowser", "sRuntimeSourceRecords", "Runtime source records"), plugin.runtimeSourceRecords);
            ImGui::Text("%s: %u", view.localize("PluginBrowser", "sRuntimeFormIDOrigins", "Runtime FormID origins"), plugin.runtimeOriginRecords);
            const auto drawOptionalCount = [&](const char* key, const char* fallback, std::optional<std::uint32_t> count) {
                const auto* label = view.localize("PluginBrowser", key, fallback);
                if (count) ImGui::Text("%s: %u", label, *count);
                else ImGui::Text("%s: %s", label, view.localize("PluginBrowser", "sDiagnosticUnavailable", "Unavailable"));
            };
            drawOptionalCount("sOverrideCount", "Overrides", plugin.overrideCount);
            drawOptionalCount("sOverriddenByOthersCount", "Overridden By Others", plugin.overriddenByOthersCount);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", view.localize("PluginBrowser", "sDiagnosticsScope",
                "Runtime-known forms only. Source is the reported file; origin is the runtime FormID owner. Override history and complete file totals are unavailable."));
            ImGui::Text("%s: %zu", view.localize("PluginBrowser", "sMasterCount", "Masters"), plugin.masters.size());

            if (plugin.missingMasters.empty()) {
                ImGui::Text("%s: %s", view.localize("PluginBrowser", "sMissingMasters", "Missing Masters"), view.localize("General", "sNo", "No"));
            } else {
                ImGui::TextColored(ImVec4(0.96f, 0.40f, 0.36f, 1.0f), "%s: %zu", view.localize("PluginBrowser", "sMissingMasters", "Missing Masters"), plugin.missingMasters.size());
                for (const auto& master : plugin.missingMasters) {
                    ImGui::BulletText("%s", master.c_str());
                }
            }

            if (!plugin.masters.empty() && ImGui::TreeNodeEx(view.localize("PluginBrowser", "sMasterList", "Master List"), ImGuiTreeNodeFlags_DefaultOpen)) {
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
                    DrawPluginDiagnosticsContent(plugin, context.view.records);
                }
                return;
            }

            ImGui::TextUnformatted(context.view.records.localize("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics"));
            ImGui::Separator();
            DrawPluginDiagnosticsContent(plugin, context.view.records);
        }

        void DrawRecordSelectable(const FormEntry& record, const char* idPrefix,
            const CatalogSnapshot& cache, Context& context, TreeFrame& frame)
        {
            const TreeRow row{ idPrefix, record.formID };
            // A popup belongs to this occurrence of the record, including its
            // section. A Selectable's label does not scope the following items.
            ImGui::PushID(idPrefix);
            ImGui::PushID(static_cast<int>(record.formID));
            const auto* displayName = record.name.empty() ? context.view.records.localize("General", "sUnnamed", "<Unnamed>") : record.name.c_str();
            const auto formIDText = FormatUtils::FormID(record.formID);
            const auto recordLabel = std::string(displayName) + " [" + formIDText + "]###" + idPrefix + formIDText;
            const bool isSelected = context.state.selection.records.selected.contains(record.formID);
            if (ImGui::Selectable(recordLabel.c_str(), isSelected)) {
                // Apply after all expanded sections supply their complete order.
                frame.click = TreeFrame::Click{ row, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift };
            }
            if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) context.requests.records.inspections.push_back(record.formID);
            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false) || (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_F10, false)))) ImGui::OpenPopup("##RecordContext");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(displayName);
                ImGui::TextDisabled("%s | %s", formIDText.c_str(), record.category.c_str());
                ImGui::TextUnformatted(record.sourcePlugin.c_str());
                if (!record.editorID.empty()) ImGui::TextUnformatted(record.editorID.c_str());
                ImGui::EndTooltip();
            }
            if (ImGui::BeginPopupContextItem("##RecordContext")) {
                if (!context.state.selection.records.selected.contains(record.formID)) {
                    context.state.selection.Single(row);
                    context.state.collapseDiagnostics = true;
                    TrackRecentRecord(record.formID, context);
                }
                frame.click.reset();
                DrawRecordContextMenu(record, true, cache, context);
                ImGui::EndPopup();
            }
            if (context.view.records.doubleClickGameplayAction && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && SupportsRecordAction(record.category, ActionKind::Give) && !record.isDeleted && context.view.records.gameplayReady) {
                context.requests.records.grants.push_back({ context.view.records.session, { record.formID } });
            }
            ImGui::PopID();
            ImGui::PopID();
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

        if (context.state.restoreScroll) ImGui::SetNextWindowScroll({0.0f, context.state.scroll});
        if (ImGui::BeginChild("PluginTreeLeft", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings)) {
            context.requests.resultsWidth = ImGui::GetWindowWidth();
            context.state.restoreScroll = false;
            context.state.scroll = ImGui::GetScrollY();
            if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
                !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
                    context.state.selection.records.selected = result.eligibleIDs;
                }
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) {
                    std::vector<std::uint32_t> ordered(context.state.selection.records.selected.begin(), context.state.selection.records.selected.end());
                    std::ranges::sort(ordered);
                    std::vector<std::string> ids; for (auto id : ordered) ids.push_back(FormatUtils::FormID(id));
                    ImGui::SetClipboardText(FormatUtils::MultiCopyList(ids, context.view.copyFormat).c_str());
                }
            }
            const std::string globalResultsHeader = CopyLabel(context.view.records.localize("PluginBrowser", "sGlobalSearchResults", "Global Search Results"), result.records.order.size(), "PluginGlobalSearchResults");
            if (context.state.globalSearch && !context.state.search.empty() && ImGui::TreeNodeEx(globalResultsHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                DrawRecordSection(result.records.order, "GlobalResult", cache, context, frame);
                if (result.records.order.empty()) ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sNoMatches", "No matching records. Clear the search or adjust the record filters."));
                ImGui::TreePop();
            }

            const std::string favoritesHeader = std::string(context.view.records.localize("General", "sFavorites", "Favorites")) + "###PluginFavorites";
            if (ImGui::TreeNodeEx(favoritesHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
                if (context.view.favoriteReviewCount) ImGui::TextWrapped("%s: %zu", context.view.records.localize("Favorites", "sReviewInSettings", "Favorites to review in Settings"), context.view.favoriteReviewCount);
                for (const auto id : context.view.records.favorites) if (result.eligibleIDs.contains(id)) frame.favorites.push_back(cache.byID.at(id));
                std::ranges::sort(frame.favorites);
                DrawRecordSection(frame.favorites, "FavoriteRecord", cache, context, frame);
                if (frame.favorites.empty()) ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sNoFavorites", "No favorites match the current search and record filters."));
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
                if (frame.recent.empty()) ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sNoRecentRecords", "No recent records match the current search and record filters."));
                ImGui::TreePop();
            }

            ImGui::Separator();
            ImGui::TextDisabled("%s: %zu", context.view.records.localize("PluginBrowser", "sPluginsCount", "Plugins"), result.plugins.size());
            if (result.plugins.empty()) ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sNoPluginMatches", "No plugins match. Clear the search, clear the plugin filter, or adjust the record filters."));

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
                const auto selectedFlag = context.state.selection.records.selected.empty() && context.state.diagnosticsPlugin == pluginName ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
                const bool pluginNodeOpen = ImGui::TreeNodeEx(pluginLabel.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | selectedFlag);
                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    context.state.diagnosticsPlugin = pluginName;
                    context.state.selection.Clear();
                    context.state.collapseDiagnostics = false;
                }
                if (!ImGui::IsItemToggledOpen() && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    context.state.diagnosticsPlugin = pluginName;
                    context.state.scope.kind = SearchScope::SelectedPlugins;
                    context.state.scope.plugins = {pluginName};
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
                        const bool categoryOpen = ImGui::TreeNodeEx(categoryLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
                        ImGui::PopStyleColor();
                        if (categoryOpen) {
                            DrawRecordSection(categoryIt->second, ("TreeRecord/" + pluginName + "/" + category).c_str(), cache, context, frame);
                            ImGui::TreePop();
                        }
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

    namespace
    {
        void DrawDetailsActions(const FormEntry& record, const CatalogSnapshot& cache, Context& context)
        {
            const auto localize = context.view.records.localize;
            const auto selected = CollectSelectedEntries(cache, context);
            const bool multiple = selected.size() > 1;
            const bool gameplayAllowed = context.view.records.gameplayReady && !record.isDeleted;
            const char* disabledTooltip = record.isDeleted ?
                localize("PluginBrowser", "sDeletedActionsDisabled", "Gameplay actions are unavailable for deleted records.") :
                localize("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");
            bool first = true;
            const auto button = [&](const char* label) { return ImGuiWidgetUtils::DrawWrappedButton(label, first); };

            if (multiple) {
                const auto giveable = CollectSelectedGiveableEntries(cache, context);
                if (!giveable.empty()) {
                    ImGui::BeginDisabled(!context.view.records.gameplayReady);
                    if (button(CopyLabel(localize("Items", "sGiveItem", "Give Item"), giveable.size(), "GiveSelection").c_str())) RequestGrant(giveable, context);
                    ImGuiWidgetUtils::ShowGameplayDisabledTooltip(context.view.records.gameplayReady,
                        localize("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open."));
                    ImGui::EndDisabled();
                }
            }

            // A record-specific popup ID prevents an open copy menu changing targets.
            const auto copyPopup = "PluginDetailsCopy/" + std::to_string(record.formID);
            if (button(localize("General", "sCopy", "Copy"))) ImGui::OpenPopup(copyPopup.c_str());
            if (ImGui::BeginPopup(copyPopup.c_str())) {
                const auto copy = [&](const char* key, const char* fallback, auto value, bool enabled = true) {
                    if (ImGui::MenuItem(localize("General", key, fallback), nullptr, false, enabled)) {
                        std::vector<std::string> values;
                        values.reserve(selected.size());
                        for (const auto index : selected) values.push_back(value(cache.records[index]));
                        const auto clipboard = multiple ? FormatUtils::MultiCopyList(values, context.view.copyFormat) : value(record);
                        ImGui::SetClipboardText(clipboard.c_str());
                    }
                };
                copy("sCopyFormID", "Copy FormID", [](const FormEntry& entry) { return FormatUtils::FormID(entry.formID); });
                copy("sCopyName", "Copy Name", [&](const FormEntry& entry) {
                    return entry.name.empty() ? std::string(localize("General", "sUnnamed", "<Unnamed>")) : entry.name;
                });
                copy("sCopyRecordSource", "Copy Record Source", [](const FormEntry& entry) { return entry.sourcePlugin; });
                copy("sCopyEditorID", "Copy EditorID", [](const FormEntry& entry) { return entry.editorID; },
                    std::ranges::any_of(selected, [&](RecordIndex index) { return !cache.records[index].editorID.empty(); }));
                ImGui::EndPopup();
            }
            if (multiple) {
                if (button(localize("General", "sFavorites", "Favorites"))) ImGui::OpenPopup("PluginSelectionFavorites");
                if (ImGui::BeginPopup("PluginSelectionFavorites")) {
                    if (ImGui::MenuItem(localize("General", "sAddFavorite", "Add Favorite"))) {
                        for (const auto index : selected) context.view.records.favorites.insert(cache.records[index].formID);
                    }
                    if (ImGui::MenuItem(localize("General", "sRemoveFavorite", "Remove Favorite"))) {
                        for (const auto index : selected) context.view.records.favorites.erase(cache.records[index].formID);
                    }
                    ImGui::EndPopup();
                }
            } else {
                const bool favorite = context.view.records.favorites.contains(record.formID);
                if (button(favorite ? localize("General", "sRemoveFavorite", "Remove Favorite") : localize("General", "sAddFavorite", "Add Favorite"))) {
                    if (favorite) context.view.records.favorites.erase(record.formID);
                    else context.view.records.favorites.insert(record.formID);
                }
            }

            if (multiple) {
                const auto popup = "PluginActiveRecord/" + std::to_string(record.formID);
                if (button(localize("PluginBrowser", "sActiveRecord", "Active record"))) ImGui::OpenPopup(popup.c_str());
                if (!ImGui::BeginPopup(popup.c_str())) return;
                ImGui::TextUnformatted(record.name.empty() ? localize("General", "sUnnamed", "<Unnamed>") : record.name.c_str());
                ImGui::TextDisabled("%s | %s", FormatUtils::FormID(record.formID).c_str(), record.category.c_str());
                ImGui::Separator();
                first = true;
            }
            ImGui::BeginDisabled(!gameplayAllowed);
            if (!multiple && SupportsRecordAction(record.category, ActionKind::Give)) {
                if (button(localize("Items", "sGiveItem", "Give Item"))) {
                    context.requests.records.grants.push_back({ context.view.records.session, { record.formID } });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayAllowed, disabledTooltip);
            }
            if (SupportsRecordAction(record.category, ActionKind::Equip)) {
                if (button(localize("General", "sEquipItem", "Equip Item"))) BrowserWidgets::Emit(context.requests.records, context.view.records, record, ActionKind::Equip);
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayAllowed, disabledTooltip);
            }
            const auto action = [&](ActionKind kind, const char* key, const char* fallback, bool confirm = true) {
                if (!SupportsRecordAction(record.category, kind)) return;
                const auto request = PrepareRecordAction(kind, record, context.view.records.session, gameplayAllowed);
                ImGui::BeginDisabled(!request);
                const auto label = std::string(localize("General", key, fallback)) + "###" + key;
                if (button(label.c_str())) BrowserWidgets::Emit(context.requests.records, context.view.records, record, kind, confirm);
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayAllowed, disabledTooltip);
                ImGui::EndDisabled();
            };
            action(ActionKind::StartQuest, "sStartQuest", "Start Quest");
            action(ActionKind::CompleteQuest, "sCompleteQuest", "Complete Quest");
            action(ActionKind::AddPerk, "sAddPerk", "Add Perk");
            action(ActionKind::RemovePerk, "sRemovePerk", "Remove Perk");
            action(ActionKind::AddSpell, "sAddSpellEffect", "Add Spell/Effect");
            action(ActionKind::RemoveSpell, "sRemoveSpellEffect", "Remove Spell/Effect");
            action(ActionKind::SetWeather, "sSetWeather", "Set Weather");
            action(ActionKind::PlaySound, "sPlaySound", "Play Sound", false);
            if (SupportsRecordAction(record.category, ActionKind::SetGlobal)) {
                ImGui::BeginDisabled(record.editorID.empty());
                if (button(localize("General", "sSetGlobal", "Set Global"))) context.requests.records.globalValues.push_back({ context.view.records.session, record.formID });
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayAllowed, disabledTooltip);
                ImGui::EndDisabled();
            }
            action(ActionKind::Outfit, "sAddOutfitItems", "Add Outfit Items");
            action(ActionKind::ConstructedItem, "sAddCraftedItem", "Add Crafted Item");
            action(ActionKind::Teleport, "sTeleportCOC", "Teleport (COC)");

            if (SupportsRecordAction(record.category, ActionKind::Spawn)) {
                first = true;
                int& quantity = context.state.spawnQuantity;
                quantity = (std::clamp)(quantity, 1, static_cast<int>(ActionQueue::MaxQuantity));
                if (button(localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                    BrowserWidgets::Emit(context.requests.records, context.view.records, record, ActionKind::Spawn, true, static_cast<std::uint32_t>(quantity));
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayAllowed, disabledTooltip);
                const auto* quantityLabel = localize("General", "sQuantity", "Quantity");
                const float quantityWidth = ImGui::GetFontSize() * 6.0f;
                ImGuiWidgetUtils::SameLineIfFits(quantityWidth + ImGui::CalcTextSize(quantityLabel).x + ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::SetNextItemWidth((std::min)(quantityWidth, ImGui::GetContentRegionAvail().x));
                const auto label = std::string(quantityLabel) + "###DetailSpawnQty";
                ImGui::InputInt(label.c_str(), &quantity, 1, 10);
                quantity = (std::clamp)(quantity, 1, static_cast<int>(ActionQueue::MaxQuantity));
            }
            ImGui::EndDisabled();
            if (multiple) ImGui::EndPopup();
        }
    }

    void DrawDiagnostics(const PluginInfo& plugin, const BrowserView& view)
    {
        DrawPluginDiagnosticsContent(plugin, view);
    }

    void DrawDetailsPane(const std::vector<PluginInfo>& plugins, const CatalogSnapshot& cache, Context& context)
    {
        if (context.view.drawInspector) {
            if (ImGui::BeginChild("PluginTreeDetails", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::Button(context.view.records.localize("PluginBrowser", "sPluginDiagnostics", "Plugin Diagnostics"))) ImGui::OpenPopup("SourceDiagnostics");
                if (ImGui::BeginPopup("SourceDiagnostics")) {
                    if (const auto* plugin = FindPluginInfo(context.state.diagnosticsPlugin, plugins)) {
                        if (ImGui::BeginChild("DiagnosticsInfo", {420, 320})) DrawPluginDiagnosticsContent(*plugin, context.view.records);
                        ImGui::EndChild();
                    } else ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sChooseDiagnosticSource", "Select a plugin in Sources or the results tree to inspect its diagnostics."));
                    ImGui::EndPopup();
                }
                context.view.drawInspector();
            }
            ImGui::EndChild();
            return;
        }
        // Only the information child scrolls. Toolbar height is established this
        // frame, so wrapping/localization never relies on a previous measurement.
        if (ImGui::BeginChild("PluginTreeDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            if (!context.state.selection.records.selected.empty()) {
                ImGui::AlignTextToFramePadding();
                if (context.state.selection.records.selected.size() == 1) {
                    ImGui::Text("%s: %s", context.view.records.localize("General", "sFormID", "FormID"),
                        FormatUtils::FormID(*context.state.selection.records.selected.begin()).c_str());
                } else {
                    ImGui::Text("%s: %zu", context.view.records.localize("General", "sSelected", "Selected"), context.state.selection.records.selected.size());
                }
                const auto* clear = context.view.records.localize("General", "sClearSelection", "Clear Selection");
                ImGuiWidgetUtils::DrawWrappedSameLine(clear);
                if (ImGui::Button(clear)) context.state.selection.Clear();
            }
            EnsurePrimarySelectionValid(context);
            const auto* record = FindRecordByFormID(cache, context.state.selection.records.active);
            const auto diagnosticName = record ? record->sourcePlugin : (!context.state.diagnosticsPlugin.empty() ? context.state.diagnosticsPlugin : std::string(context.view.records.pluginFilter));
            const auto* plugin = diagnosticName.empty() ? nullptr : FindPluginInfo(diagnosticName, plugins);
            const auto formID = record ? record->formID : 0;
            const bool changed = context.state.detailsFormID != formID || context.state.detailsPlugin != diagnosticName;
            context.state.detailsFormID = formID;
            context.state.detailsPlugin = diagnosticName;

            if (record) {
                DrawDetailsActions(*record, cache, context);
                ImGui::Separator();
                context.requests.details = DetailKey{ formID, context.view.records.session, cache.generation, context.view.advancedDetails };
            }
            if (changed) ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
            if (ImGui::BeginChild("PluginDetailsInfo", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (record) {
                    const FormDetailsViewContext detailsContext{
                        .localize = context.view.records.localize,
                        .showAdvancedDetailsView = context.view.advancedDetails,
                        .catalog = &cache,
                        .details = context.view.details && context.view.details->key == *context.requests.details ? context.view.details : nullptr
                    };
                    FormDetailsView::Draw(*record, detailsContext);
                    if (plugin) {
                        ImGui::Spacing();
                        DrawPluginDiagnosticsSection(*plugin, context, true);
                    }
                } else if (plugin) {
                    DrawPluginDiagnosticsSection(*plugin, context, false);
                } else {
                    ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
                    ImGui::Spacing();
                    ImGui::TextWrapped("%s", context.view.records.localize("PluginBrowser", "sSelectionHint", "Ctrl-click to toggle records; Shift-click to select a range."));
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
}
