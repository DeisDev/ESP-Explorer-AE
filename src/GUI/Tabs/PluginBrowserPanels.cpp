#include "GUI/Tabs/PluginBrowserPanels.h"

#include "Config/Config.h"

#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormDetailsView.h"
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
            char recordLabel[512]{};
            std::snprintf(recordLabel, sizeof(recordLabel), "%s [%08X]##%s%08X", displayName, record.formID, idPrefix, record.formID);
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
                EnsurePrimarySelectionValid(context);
            }

            if (ImGui::BeginPopupContextItem()) {
                if (!isSelected) {
                    context.selectedPluginTreeRecordFormIDs.clear();
                    context.selectedPluginTreeRecordFormIDs.insert(record.formID);
                    context.selectedPluginTreeRecordFormID = record.formID;
                    context.pluginTreeLastClickedFormID = record.formID;
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

        void DrawWrappedActionButtonRowState(bool& firstBtn, int& buttonsInRow, float& buttonWidth)
        {
            if (!firstBtn) {
                return;
            }
            buttonsInRow = 0;
            buttonWidth = 0.0f;
        }

        bool WrappedButton(const char* label, bool& firstInRow, int& buttonsInRow, float& buttonWidth)
        {
            const auto& style = ImGui::GetStyle();
            constexpr int kButtonsPerRow = 3;

            if (buttonsInRow == 0) {
                const float rowAvailable = ImGui::GetContentRegionAvail().x;
                buttonWidth = (std::max)(96.0f, (rowAvailable - style.ItemSpacing.x * static_cast<float>(kButtonsPerRow - 1)) / static_cast<float>(kButtonsPerRow));
            }

            if (buttonsInRow > 0) {
                ImGui::SameLine();
            }

            const bool pressed = ImGui::Button(label, ImVec2(buttonWidth, 0.0f));
            ++buttonsInRow;
            if (buttonsInRow >= kButtonsPerRow) {
                buttonsInRow = 0;
                firstInRow = true;
            } else {
                firstInRow = false;
            }

            return pressed;
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

    void DrawDetailsPane(const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
        if (ImGui::BeginChild("PluginTreeDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            EnsurePrimarySelectionValid(context);
            const FormEntry* selectedRecord = context.selectedPluginTreeRecordFormID != 0 ? FindRecordByFormID(cache, context.selectedPluginTreeRecordFormID, dataVersion) : nullptr;
            const auto selectedEntries = CollectSelectedEntries(cache, dataVersion, context);
            const auto selectedGiveableEntries = CollectSelectedGiveableEntries(cache, dataVersion, context);
            const bool hasMultipleSelection = context.selectedPluginTreeRecordFormIDs.size() > 1;

            if (!selectedRecord) {
                ImGui::TextUnformatted(context.localize("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
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

                    bool firstBtn = true;
                    int buttonsInRow = 0;
                    float buttonWidth = 0.0f;

                    if (hasMultipleSelection && !selectedEntries.empty()) {
                        if (WrappedButton((std::string(context.localize("General", "sCopyName", "Copy Name")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str(), firstBtn, buttonsInRow, buttonWidth)) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                values.push_back(selectedEntry.name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                            }
                            const auto text = SharedUtils::BuildParenthesizedList(values);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (WrappedButton((std::string(context.localize("General", "sCopyFormID", "Copy FormID")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str(), firstBtn, buttonsInRow, buttonWidth)) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                char formBuffer[16]{};
                                std::snprintf(formBuffer, sizeof(formBuffer), "%08X", selectedEntry.formID);
                                values.emplace_back(formBuffer);
                            }
                            const auto text = SharedUtils::BuildParenthesizedList(values);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (WrappedButton((std::string(context.localize("General", "sCopyRecordSource", "Copy Record Source")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str(), firstBtn, buttonsInRow, buttonWidth)) {
                            std::vector<std::string> values{};
                            values.reserve(selectedEntries.size());
                            for (const auto& selectedEntry : selectedEntries) {
                                values.push_back(selectedEntry.sourcePlugin);
                            }
                            const auto text = SharedUtils::BuildParenthesizedList(values);
                            ImGui::SetClipboardText(text.c_str());
                        }

                        if (WrappedButton((std::string(context.localize("General", "sAddFavorite", "Add Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str(), firstBtn, buttonsInRow, buttonWidth)) {
                            for (const auto& selectedEntry : selectedEntries) {
                                context.favoriteForms.insert(selectedEntry.formID);
                            }
                        }

                        if (WrappedButton((std::string(context.localize("General", "sRemoveFavorite", "Remove Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str(), firstBtn, buttonsInRow, buttonWidth)) {
                            for (const auto& selectedEntry : selectedEntries) {
                                context.favoriteForms.erase(selectedEntry.formID);
                            }
                        }
                    } else {
                        if (WrappedButton(context.localize("General", "sCopyFormID", "Copy FormID"), firstBtn, buttonsInRow, buttonWidth)) {
                            FormActions::CopyFormID(selectedRecord->formID);
                        }

                        if (WrappedButton(context.localize("General", "sCopyRecordSource", "Copy Record Source"), firstBtn, buttonsInRow, buttonWidth)) {
                            ImGui::SetClipboardText(selectedRecord->sourcePlugin.c_str());
                        }

                        if (WrappedButton(context.localize("General", "sCopyName", "Copy Name"), firstBtn, buttonsInRow, buttonWidth)) {
                            ImGui::SetClipboardText(selectedRecord->name.empty() ? context.localize("General", "sUnnamed", "<Unnamed>") : selectedRecord->name.c_str());
                        }

                        if (canGive) {
                            if (WrappedButton(context.localize("Items", "sGiveItem", "Give Item"), firstBtn, buttonsInRow, buttonWidth)) {
                                context.openItemGrantPopup(*selectedRecord);
                            }
                        }

                        if (isEquippable) {
                            if (WrappedButton(context.localize("General", "sEquipItem", "Equip Item"), firstBtn, buttonsInRow, buttonWidth)) {
                                EquipRecordWithConfiguredAmmo(*selectedRecord, context.equipWeaponAmmoCount);
                            }
                        }

                        const bool isFavorite = context.favoriteForms.contains(selectedRecord->formID);
                        if (WrappedButton(isFavorite ? context.localize("General", "sRemoveFavorite", "Remove Favorite") : context.localize("General", "sAddFavorite", "Add Favorite"), firstBtn, buttonsInRow, buttonWidth)) {
                            if (isFavorite) {
                                context.favoriteForms.erase(selectedRecord->formID);
                            } else {
                                context.favoriteForms.insert(selectedRecord->formID);
                            }
                        }
                    }

                    if (hasMultipleSelection && !selectedGiveableEntries.empty()) {
                        std::string giveSelectedLabel = std::string(context.localize("Items", "sGiveItem", "Give Item")) + " (" + std::to_string(selectedGiveableEntries.size()) + ")";
                        if (WrappedButton(giveSelectedLabel.c_str(), firstBtn, buttonsInRow, buttonWidth)) {
                            context.openItemGrantPopupMultiple(selectedGiveableEntries);
                        }
                    }

                    if (canSpawn || canGive) {
                        DrawWrappedActionButtonRowState(firstBtn, buttonsInRow, buttonWidth);
                        const char* spawnLabel = context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player");

                        static int detailSpawnQuantity = 1;
                        if (WrappedButton(spawnLabel, firstBtn, buttonsInRow, buttonWidth)) {
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
                        buttonsInRow = 0;
                        buttonWidth = 0.0f;
                    }

                    if (isQuest) {
                        if (WrappedButton(context.localize("General", "sStartQuest", "Start Quest"), firstBtn, buttonsInRow, buttonWidth)) {
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
                        if (WrappedButton(context.localize("General", "sCompleteQuest", "Complete Quest"), firstBtn, buttonsInRow, buttonWidth)) {
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
                    }

                    if (isPerk) {
                        if (WrappedButton(context.localize("General", "sAddPerk", "Add Perk"), firstBtn, buttonsInRow, buttonWidth)) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddPerk", "Add selected perk to player?"),
                                [formID]() {
                                    FormActions::AddPerkToPlayer(formID);
                                });
                        }
                        if (WrappedButton(context.localize("General", "sRemovePerk", "Remove Perk"), firstBtn, buttonsInRow, buttonWidth)) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmRemovePerk", "Remove selected perk from player?"),
                                [formID]() {
                                    FormActions::RemovePerkFromPlayer(formID);
                                });
                        }
                    }

                    if (isSpellLike) {
                        if (WrappedButton(context.localize("General", "sAddSpellEffect", "Add Spell/Effect"), firstBtn, buttonsInRow, buttonWidth)) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddSpellEffect", "Add selected spell/effect to player?"),
                                [formID]() {
                                    FormActions::AddSpellToPlayer(formID);
                                });
                        }
                        if (WrappedButton(context.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"), firstBtn, buttonsInRow, buttonWidth)) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmRemoveSpellEffect", "Remove selected spell/effect from player?"),
                                [formID]() {
                                    FormActions::RemoveSpellFromPlayer(formID);
                                });
                        }
                    }

                    if (isWeather) {
                        if (WrappedButton(context.localize("General", "sSetWeather", "Set Weather"), firstBtn, buttonsInRow, buttonWidth)) {
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
                    }

                    if (isSound) {
                        if (WrappedButton(context.localize("General", "sPlaySound", "Play Sound"), firstBtn, buttonsInRow, buttonWidth)) {
                            if (const char* editorID = ContextMenu::TryGetEditorID(selectedRecord->formID)) {
                                std::string command = std::string("playsound ") + editorID;
                                FormActions::ExecuteConsoleCommand(command);
                            }
                        }
                    }

                    if (isGlobal) {
                        if (WrappedButton(context.localize("General", "sSetGlobal", "Set Global"), firstBtn, buttonsInRow, buttonWidth)) {
                            context.openGlobalValuePopup(selectedRecord->formID);
                        }
                    }

                    if (isOutfit) {
                        if (WrappedButton(context.localize("General", "sAddOutfitItems", "Add Outfit Items"), firstBtn, buttonsInRow, buttonWidth)) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddOutfitItems", "Add all items from selected outfit to player?"),
                                [formID]() {
                                    FormActions::AddOutfitItemsToPlayer(formID);
                                });
                        }
                    }

                    if (isConstructible) {
                        if (WrappedButton(context.localize("General", "sAddCraftedItem", "Add Crafted Item"), firstBtn, buttonsInRow, buttonWidth)) {
                            const auto formID = selectedRecord->formID;
                            context.requestActionConfirmation(
                                context.localize("General", "sConfirmAction", "Confirm Action"),
                                context.localize("General", "sConfirmAddCraftedItem", "Add crafted output of selected recipe to player?"),
                                [formID]() {
                                    FormActions::AddConstructedItemToPlayer(formID);
                                });
                        }
                    }

                    if (canTeleport) {
                        auto* teleportForm = RE::TESForm::GetFormByID(selectedRecord->formID);
                        const char* editorID = teleportForm ? teleportForm->GetFormEditorID() : nullptr;
                        const bool canUseCoc = editorID && editorID[0] != '\0';

                        if (canUseCoc) {
                            if (WrappedButton(context.localize("General", "sTeleportCOC", "Teleport (COC)"), firstBtn, buttonsInRow, buttonWidth)) {
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
                            WrappedButton(context.localize("General", "sTeleportCOC", "Teleport (COC)"), firstBtn, buttonsInRow, buttonWidth);
                            ImGui::EndDisabled();
                        }
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::EndChild();
    }
}