#include "Core/Profiling.h"
#include "GUI/Tabs/InventoryTab.h"


#include "Core/RecordActions.h"
#include "Core/StandardForms.h"
#include "GUI/Widgets/ActionFeedback.h"
#include "GUI/Widgets/InventoryFeedback.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/FormDetailsView.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>


#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <map>
#include <ranges>
#include <unordered_map>

namespace ESPExplorerAE
{
    namespace
    {
        class InventoryView
        {
            InventoryTabState& state;
            const InventoryTabView& view;
            InventoryTabRequests& requests;
        public:
            InventoryView(InventoryTabState& value, const InventoryTabView& input, InventoryTabRequests& output) : state(value), view(input), requests(output) {}

            const char* L(const InventoryTabView& context, std::string_view section, std::string_view key, const char* fallback)
            {
                return context.localize ? context.localize(section, key, fallback) : fallback;
            }

            std::uint64_t MakeRowKey(const InventoryEntry& entry) { return entry.groupID; }

            bool IsWeaponCategory(std::string_view category)
            {
                return category == "WEAP";
            }

            bool IsArmorCategory(std::string_view category)
            {
                return category == "ARMO";
            }

            bool IsAmmoCategory(std::string_view category)
            {
                return category == "AMMO";
            }

            bool IsAidCategory(std::string_view category)
            {
                return category == "ALCH";
            }

            bool IsMiscCategory(std::string_view category)
            {
                return category == "MISC" || category == "OMOD";
            }

            bool IsKeyCategory(std::string_view category)
            {
                return category == "KEYM";
            }

            bool IsNoteCategory(std::string_view category)
            {
                return category == "BOOK" || category == "NOTE";
            }

            bool IsComponentCategory(std::string_view category)
            {
                return category == "CMPO";
            }

            bool IsJunkCategory(std::string_view category)
            {
                return category == "JUNK";
            }

            bool IsEquippable(const InventoryEntry& entry)
            {
                return IsWeaponCategory(entry.category) || IsArmorCategory(entry.category);
            }

            std::string ResolveCategoryLabel(std::string_view category, const InventoryTabView& context)
            {
                if (category == "WEAP") {
                    return L(context, "Inventory", "sWeapons", "Weapons");
                }
                if (category == "ARMO") {
                    return L(context, "Inventory", "sArmor", "Armor");
                }
                if (category == "AMMO") {
                    return L(context, "Inventory", "sAmmo", "Ammo");
                }
                if (category == "ALCH") {
                    return L(context, "Inventory", "sAid", "Aid");
                }
                if (category == "KEYM") {
                    return L(context, "Inventory", "sKeys", "Keys");
                }
                if (category == "BOOK") {
                    return L(context, "Inventory", "sNotes", "Notes/Holotapes");
                }
                if (category == "CMPO") {
                    return L(context, "Inventory", "sComponents", "Components");
                }
                if (category == "JUNK") {
                    return L(context, "Inventory", "sJunk", "Junk");
                }
                if (category == "MISC" || category == "OMOD") {
                    return L(context, "Inventory", "sMisc", "Misc");
                }
                return std::string(category);
            }

            void RefreshPlayerInventory(const InventoryTabView& context)
            {
                const auto& next = context.inventory;
                const bool unchanged = state.snapshot && next && state.snapshot->session == next->session && state.snapshot->generation == next->generation;
                if (unchanged && state.catalogGeneration == context.catalog->generation) return;
                if (!unchanged) {
                    state.selectionChanged = !state.selection.selected.empty();
                    state.selection.Clear();
                    state.instanceChoices.clear();
                    state.desiredCounts.clear();
                }
                state.catalogGeneration = context.catalog->generation;
                state.snapshot = next;
                state.cachedInventory.clear();
                if (!next || !next->ready) return;
                for (const auto& group : next->groups) {
                    const auto& stack = next->stacks[group.representative];
                    InventoryEntry entry;
                    entry.formID = stack.formID;
                    entry.name = stack.name;
                    entry.category = stack.category;
                    entry.sourcePlugin = stack.sourcePlugin;
                    entry.count = group.count;
                    entry.weight = stack.weight;
                    entry.value = stack.value;
                    entry.isEquipped = group.anyEquipped;
                    entry.isFavorited = group.anyFavorited;
                    entry.isLegendary = group.anyLegendary;
                    entry.isQuestItem = group.anyQuestItem;
                    entry.modCount = stack.modCount;
                    entry.damage = stack.damage;
                    entry.armorRating = stack.armorRating;
                    entry.groupID = group.id;
                    entry.source = next;
                    entry.stacks = group.stacks;
                    entry.representative = group.representative;
                    if (const auto* record = context.catalog->Find(stack.formID)) entry.editorID = record->editorID;
                    state.cachedInventory.push_back(std::move(entry));
                }
            }

            bool MatchesCategory(const InventoryEntry& entry, InventoryCategoryTab category)
            {
                switch (category) {
                case InventoryCategoryTab::All:
                    return true;
                case InventoryCategoryTab::Weapons:
                    return IsWeaponCategory(entry.category);
                case InventoryCategoryTab::Armor:
                    return IsArmorCategory(entry.category);
                case InventoryCategoryTab::Ammo:
                    return IsAmmoCategory(entry.category);
                case InventoryCategoryTab::Aid:
                    return IsAidCategory(entry.category);
                case InventoryCategoryTab::Misc:
                    return IsMiscCategory(entry.category);
                case InventoryCategoryTab::Keys:
                    return IsKeyCategory(entry.category);
                case InventoryCategoryTab::Notes:
                    return IsNoteCategory(entry.category);
                case InventoryCategoryTab::Components:
                    return IsComponentCategory(entry.category);
                case InventoryCategoryTab::Junk:
                    return IsJunkCategory(entry.category);
                }
                return true;
            }

            std::vector<const InventoryEntry*> BuildVisibleEntries()
            {
                std::vector<const InventoryEntry*> visibleEntries{};
                visibleEntries.reserve(state.cachedInventory.size());

                for (const auto& entry : state.cachedInventory) {
                    if (!MatchesCategory(entry, state.activeCategory)) {
                        continue;
                    }
                    if (state.showEquippedOnly && !entry.isEquipped) {
                        continue;
                    }
                    if (!state.inventorySearch.empty()) {
                        const std::string formIDText = FormatUtils::FormID(entry.formID);
                        if (!SharedUtils::ContainsCaseInsensitive(entry.name, state.inventorySearch) &&
                            !SharedUtils::ContainsCaseInsensitive(entry.sourcePlugin, state.inventorySearch) &&
                            !SharedUtils::ContainsCaseInsensitive(entry.category, state.inventorySearch) &&
                            !SharedUtils::ContainsCaseInsensitive(formIDText, state.inventorySearch)) {
                            if (!SharedUtils::ContainsCaseInsensitive(entry.editorID, state.inventorySearch)) {
                                continue;
                            }
                        }
                    }

                    visibleEntries.push_back(&entry);
                }

                return visibleEntries;
            }

            int CompareInventoryEntries(const InventoryEntry& left, const InventoryEntry& right, int columnIndex)
            {
                switch (columnIndex) {
                case 0:
                    return left.name.compare(right.name);
                case 1:
                    return left.category.compare(right.category);
                case 2:
                    return left.count < right.count ? -1 : (left.count > right.count ? 1 : 0);
                case 3:
                    return left.value < right.value ? -1 : (left.value > right.value ? 1 : 0);
                case 4:
                    return left.weight < right.weight ? -1 : (left.weight > right.weight ? 1 : 0);
                case 5: {
                    const std::uint16_t leftMetric = IsArmorCategory(left.category) ? left.armorRating : left.damage;
                    const std::uint16_t rightMetric = IsArmorCategory(right.category) ? right.armorRating : right.damage;
                    return leftMetric < rightMetric ? -1 : (leftMetric > rightMetric ? 1 : 0);
                }
                case 6:
                    return left.modCount < right.modCount ? -1 : (left.modCount > right.modCount ? 1 : 0);
                case 7:
                    return left.sourcePlugin.compare(right.sourcePlugin);
                default:
                    return 0;
                }
            }

            void SortVisibleEntries(std::vector<const InventoryEntry*>& visibleEntries, ImGuiTableSortSpecs* sortSpecs)
            {
                if (!sortSpecs || sortSpecs->SpecsCount == 0) {
                    std::ranges::sort(visibleEntries, [](const InventoryEntry* left, const InventoryEntry* right) {
                        if (left->name != right->name) {
                            return left->name < right->name;
                        }
                        if (left->formID != right->formID) {
                            return left->formID < right->formID;
                        }
                        return left->groupID < right->groupID;
                    });
                    return;
                }

                std::ranges::sort(visibleEntries, [&](const InventoryEntry* left, const InventoryEntry* right) {
                    for (int specIndex = 0; specIndex < sortSpecs->SpecsCount; ++specIndex) {
                        const auto& spec = sortSpecs->Specs[specIndex];
                        const int cmp = CompareInventoryEntries(*left, *right, spec.ColumnIndex);
                        if (cmp == 0) {
                            continue;
                        }

                        return spec.SortDirection == ImGuiSortDirection_Ascending ? (cmp < 0) : (cmp > 0);
                    }

                    if (left->formID != right->formID) {
                        return left->formID < right->formID;
                    }
                    return left->groupID < right->groupID;
                });
            }

            void MarkRefreshNeeded() { requests.refresh = true; }

            std::optional<InventoryIndex> SelectedInstance(const InventoryEntry& entry)
            {
                if (entry.stacks.size() == 1) return entry.stacks.front();
                const auto choice = state.instanceChoices.find(entry.groupID);
                if (choice == state.instanceChoices.end()) return {};
                for (const auto index : entry.stacks) if (entry.source->stacks[index].token == choice->second) return index;
                return {};
            }

            InventoryPreparation PrepareEntries(std::span<const InventoryEntry> entries, InventoryAction action, std::optional<std::uint64_t> quantity = {})
            {
                if (entries.empty()) return {};
                const auto source = entries.front().source;
                std::vector<InventoryIndex> targets;
                std::uint64_t count{};
                const bool destructive = action == InventoryAction::Remove || action == InventoryAction::Drop;
                for (const auto& entry : entries) {
                    if (entry.source != source) return {};
                    if (destructive) { targets.insert(targets.end(), entry.stacks.begin(), entry.stacks.end()); count += entry.count; }
                    else {
                        const auto target = SelectedInstance(entry);
                        if (!target) return { InventoryRejection::AmbiguousInstance, {} };
                        targets.push_back(*target);
                        ++count;
                    }
                }
                if (!destructive && targets.size() > 1) {
                    InventoryPreparation combined{ InventoryRejection::None, { source, action, {} } };
                    for (const auto index : targets) {
                        const auto one = PrepareInventoryAction(source, std::span{ &index, 1 }, action, 1);
                        if (!one) return one;
                        combined.plan.steps.push_back(one.plan.steps.front());
                    }
                    if (combined.plan.steps.size() > ActionQueue::Capacity) return { InventoryRejection::TooLarge, {} };
                    return combined;
                }
                return PrepareInventoryAction(source, targets, action, quantity.value_or(count));
            }

            bool EmitAction(ActionRequest request)
            {
                if (!view.gameplayReady) { state.admission = ActionAdmission::Unavailable; return false; }
                if (!request.session) request.session = view.session;
                if (request.session != view.session) { state.admission = ActionAdmission::StaleSession; return false; }
                if (!ActionQueue::Valid(request)) { state.admission = ActionAdmission::Invalid; return false; }
                requests.records.actions.push_back({ std::move(request), false, {} });
                return true;
            }

            bool SubmitPrepared(const InventoryPreparation& prepared)
            {
                state.rejection = prepared.rejection;
                if (!prepared) return false;
                if (prepared.plan.source->session != view.session) { state.admission = ActionAdmission::StaleSession; return false; }
                if (!view.gameplayReady) { state.admission = ActionAdmission::Unavailable; return false; }
                for (auto request : InventoryRequests(prepared)) requests.records.actions.push_back({ std::move(request), false, {} });
                return true;
            }

            void ConfirmEntries(std::span<const InventoryEntry> entries, InventoryAction action, std::string title, std::string message)
            {
                const auto prepared = PrepareEntries(entries, action);
                state.rejection = prepared.rejection;
                if (!prepared) return;
                if (prepared.plan.source->session != view.session) { state.admission = ActionAdmission::StaleSession; return; }
                if (!view.gameplayReady) { state.admission = ActionAdmission::Unavailable; return; }
                requests.confirmations.push_back({ std::move(title), std::move(message), InventoryRequests(prepared) });
            }

            bool AddBaseItems(const InventoryEntry& entry, std::uint32_t count)
            {
                state.rejection = ValidateInventoryBaseAddition(entry.source, entry.stacks, count);
                if (state.rejection != InventoryRejection::None) return false;
                return EmitAction({ .kind = ActionKind::InventoryAddBase, .formID = entry.formID, .count = count, .session = entry.source->session,
                    .inventory = entry.source, .inventoryGroup = entry.stacks });
            }

            bool RemoveInventoryEntry(const InventoryEntry& entry, std::uint64_t count, bool drop)
            {
                return SubmitPrepared(PrepareEntries(std::span{ &entry, 1 }, drop ? InventoryAction::Drop : InventoryAction::Remove, count));
            }

            bool AdjustInventoryEntryCount(const InventoryEntry& entry, std::int32_t desiredCount)
            {
                if (desiredCount < 0) return false;
                const auto desired = static_cast<std::uint64_t>(desiredCount);
                if (desired == entry.count) return true;
                if (desired < entry.count) return RemoveInventoryEntry(entry, entry.count - desired, false);
                return AddBaseItems(entry, static_cast<std::uint32_t>(desired - entry.count));
            }

            bool EquipInventoryEntry(const InventoryEntry& entry, bool equip)
            {
                return SubmitPrepared(PrepareEntries(std::span{ &entry, 1 }, equip ? InventoryAction::Equip : InventoryAction::Unequip));
            }

            bool UseInventoryEntry(const InventoryEntry& entry)
            {
                return SubmitPrepared(PrepareEntries(std::span{ &entry, 1 }, InventoryAction::Use));
            }

            ImVec4 GetCategoryColor(std::string_view category, bool isQuestItem, bool isEquipped)
            {
                const auto& textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                const float textLuma = 0.299f * textColor.x + 0.587f * textColor.y + 0.114f * textColor.z;
                const bool darkTheme = textLuma > 0.5f;

                if (isQuestItem) {
                    return darkTheme ? ImVec4(1.0f, 0.84f, 0.0f, 1.0f) : ImVec4(0.7f, 0.55f, 0.0f, 1.0f);
                }

                ImVec4 color = textColor;
                if (category == "WEAP") {
                    color = darkTheme ? ImVec4(0.95f, 0.40f, 0.35f, 1.0f) : ImVec4(0.75f, 0.20f, 0.15f, 1.0f);
                } else if (category == "ARMO") {
                    color = darkTheme ? ImVec4(0.40f, 0.60f, 1.0f, 1.0f) : ImVec4(0.15f, 0.30f, 0.75f, 1.0f);
                } else if (category == "AMMO") {
                    color = darkTheme ? ImVec4(0.95f, 0.75f, 0.25f, 1.0f) : ImVec4(0.65f, 0.50f, 0.05f, 1.0f);
                } else if (category == "ALCH") {
                    color = darkTheme ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.10f, 0.55f, 0.20f, 1.0f);
                } else if (category == "KEYM") {
                    color = darkTheme ? ImVec4(0.90f, 0.80f, 0.35f, 1.0f) : ImVec4(0.60f, 0.50f, 0.10f, 1.0f);
                } else if (category == "BOOK") {
                    color = darkTheme ? ImVec4(0.45f, 0.85f, 0.90f, 1.0f) : ImVec4(0.10f, 0.50f, 0.55f, 1.0f);
                } else if (category == "CMPO") {
                    color = darkTheme ? ImVec4(0.75f, 0.55f, 0.95f, 1.0f) : ImVec4(0.45f, 0.25f, 0.70f, 1.0f);
                } else if (category == "JUNK") {
                    color = darkTheme ? ImVec4(0.70f, 0.60f, 0.45f, 1.0f) : ImVec4(0.45f, 0.35f, 0.20f, 1.0f);
                } else if (category == "MISC" || category == "OMOD") {
                    color = darkTheme ? ImVec4(0.75f, 0.75f, 0.75f, 1.0f) : ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
                }

                if (isEquipped) {
                    color.x = (std::min)(color.x * 1.15f, 1.0f);
                    color.y = (std::min)(color.y * 1.15f, 1.0f);
                    color.z = (std::min)(color.z * 1.15f, 1.0f);
                }

                return color;
            }

            float CalculateTotalWeight()
            {
                double total{};
                if (state.snapshot) for (const auto& group : state.snapshot->groups) if (std::isfinite(group.totalWeight)) total += group.totalWeight;
                return static_cast<float>(total);
            }

            void DrawQuickActions(const InventoryTabView& context)
            {
                if (!ImGui::CollapsingHeader(L(context, "Inventory", "sQuickActions", "Quick Actions"))) {
                    return;
                }

                auto wrappedSameLine = [](const char* nextLabel) {
                    ImGuiWidgetUtils::DrawWrappedSameLine(nextLabel);
                };

                const bool gameplayActionsAllowed = context.gameplayReady;
                const char* disabledTooltip = L(context, "General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");

                if (!gameplayActionsAllowed) {
                    ImGui::BeginDisabled(true);
                }

                SharedUtils::DrawSectionLabel(L(context, "Inventory", "sCharacterSection", "Character"));

                if (ImGui::Button(L(context, "Inventory", "sRefillHealth", "Refill Health"))) {
                    EmitAction({ .kind = ActionKind::RestoreHealth });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                const char* godModeLabel = context.godMode ? L(context, "Inventory", "sGodModeOn", "Godmode: ON") : L(context, "Inventory", "sGodModeOff", "Godmode: OFF");
                wrappedSameLine(godModeLabel);
                if (ImGui::Button(godModeLabel)) {
                    EmitAction({ .kind = ActionKind::GodMode, .ammoCount = context.godMode ? 0u : 1u });
                    }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                const char* noClipLabel = L(context, "Inventory", "sToggleNoClip", "Toggle Noclip");
                wrappedSameLine(noClipLabel);
                if (ImGui::Button(noClipLabel)) {
                    EmitAction({ .kind = ActionKind::ToggleNoClip });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                const float inputWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x * 0.2f);

                ImGui::SetNextItemWidth(inputWidth);
                ImGui::InputInt(L(context, "Inventory", "sSetLevel", "Set Level"), &state.quick.level, 1, 10);
                state.quick.level = (std::clamp)(state.quick.level, 1, 65535);
                const char* applyLevelLabel = L(context, "Inventory", "sApplyLevel", "Apply Level");
                wrappedSameLine(applyLevelLabel);
                if (ImGui::Button(applyLevelLabel)) {
                    EmitAction({ .kind = ActionKind::SetPlayerLevel, .count = static_cast<std::uint32_t>(state.quick.level) });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                ImGui::SetNextItemWidth(inputWidth);
                ImGui::InputInt(L(context, "Inventory", "sAddPerkPoints", "Perk Points"), &state.quick.perkPoints, 1, 5);
                state.quick.perkPoints = (std::clamp)(state.quick.perkPoints, 1, 999);
                const char* addPerkPointsLabel = L(context, "Inventory", "sAddPerkPointsBtn", "Add Perk Points");
                wrappedSameLine(addPerkPointsLabel);
                if (ImGui::Button(addPerkPointsLabel)) {
                    EmitAction({ .kind = ActionKind::AddPerkPoints, .count = static_cast<std::uint32_t>(state.quick.perkPoints) });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                SharedUtils::DrawSectionLabel(L(context, "Inventory", "sAmmunitionSection", "Ammunition"));

                ImGui::SetNextItemWidth(inputWidth);
                ImGui::InputInt(L(context, "Inventory", "sCurrentWeaponAmmo", "Current Weapon Ammo"), &state.quick.currentAmmo, 10, 100);
                state.quick.currentAmmo = (std::clamp)(state.quick.currentAmmo, 1, static_cast<int>(ActionQueue::MaxQuantity));
                const char* addCurrentAmmoLabel = L(context, "Inventory", "sAddCurrentAmmo", "Add Ammo For Held Weapon");
                wrappedSameLine(addCurrentAmmoLabel);
                if (ImGui::Button(addCurrentAmmoLabel)) {
                    EmitAction({ .kind = ActionKind::CurrentAmmo, .count = static_cast<std::uint32_t>(state.quick.currentAmmo) });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                ImGui::SetNextItemWidth(inputWidth);
                ImGui::InputInt(L(context, "Inventory", "sAllAmmoCount", "All Ammo Count"), &state.quick.allAmmo, 10, 100);
                state.quick.allAmmo = (std::clamp)(state.quick.allAmmo, 1, static_cast<int>(ActionQueue::MaxQuantity));
                const char* addAllAmmoLabel = L(context, "Inventory", "sAddAllAmmo", "Add All Ammo Types");
                wrappedSameLine(addAllAmmoLabel);
                if (ImGui::Button(addAllAmmoLabel)) {
                    if (const auto ammo = context.catalog->byType.find("AMMO"); ammo != context.catalog->byType.end()) {
                        std::vector<ActionRequest> batch;
                        bool valid = true;
                        for (const auto index : ammo->second) {
                            auto request = PrepareRecordAction(ActionKind::Give, context.catalog->records[index], context.session, gameplayActionsAllowed,
                                static_cast<std::uint32_t>(state.quick.allAmmo));
                            if (!request) { valid = false; break; }
                            batch.push_back(std::move(*request));
                        }
                        if (valid) for (auto& request : batch) requests.records.actions.push_back({ std::move(request), false, {} });
                        else state.admission = ActionAdmission::Invalid;
                    }
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                SharedUtils::DrawSectionLabel(L(context, "Inventory", "sQuickItemsSection", "Quick Items"));

                if (ImGui::Button(L(context, "Inventory", "sAddStimpak", "Add Stimpaks"))) {
                    requests.records.grants.push_back({ context.session, { StandardForms::Stimpak } });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                const char* lockpickLabel = L(context, "Inventory", "sAddLockpick", "Add Lockpicks");
                wrappedSameLine(lockpickLabel);
                if (ImGui::Button(lockpickLabel)) {
                    requests.records.grants.push_back({ context.session, { StandardForms::Lockpick } });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                const char* capsLabel = L(context, "Inventory", "sAddCaps", "Add Caps");
                wrappedSameLine(capsLabel);
                if (ImGui::Button(capsLabel)) {
                    requests.records.grants.push_back({ context.session, { StandardForms::Caps } });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                SharedUtils::DrawSectionLabel(L(context, "Inventory", "sTimeOfDaySection", "Time of Day"));

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
                    int timeOfDayHour = static_cast<int>(state.quick.gameHour + 0.5f);
                    if (ImGui::SliderInt(
                            L(context, "Inventory", "sTimeOfDaySlider", "Hour"),
                            &timeOfDayHour, 0, 23, "%d")) {
                        state.quick.gameHour = static_cast<float>(timeOfDayHour);
                    }

                const char* morningLabel = L(context, "Inventory", "sTimeMorning", "Morning");
                const char* noonLabel = L(context, "Inventory", "sTimeNoon", "Noon");
                const char* eveningLabel = L(context, "Inventory", "sTimeEvening", "Evening");
                const char* midnightLabel = L(context, "Inventory", "sTimeMidnight", "Midnight");
                const char* applyTimeLabel = L(context, "Inventory", "sApplyTime", "Set Time");

                if (ImGui::Button(morningLabel)) { state.quick.gameHour = 6.0f; }
                ImGui::SameLine();
                if (ImGui::Button(noonLabel)) { state.quick.gameHour = 12.0f; }
                ImGui::SameLine();
                if (ImGui::Button(eveningLabel)) { state.quick.gameHour = 18.0f; }
                ImGui::SameLine();
                if (ImGui::Button(midnightLabel)) { state.quick.gameHour = 0.0f; }
                wrappedSameLine(applyTimeLabel);
                if (ImGui::Button(applyTimeLabel)) {
                    EmitAction({ .kind = ActionKind::SetGameHour, .value = state.quick.gameHour });
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

                if (!gameplayActionsAllowed) {
                    ImGui::EndDisabled();
                }
            }

            void DrawSummaryBar(const InventoryTabView& context)
            {
                const float totalWeight = CalculateTotalWeight();
                const float carryWeight = context.inventory ? context.inventory->carryWeight : 0.0f;
                const float ratio = carryWeight > 0.0f ? totalWeight / carryWeight : 0.0f;

                ImGui::TextDisabled(
                    "%s: %zu  |  %s: %.1f  |  %s: %.1f / %.1f  |  %s: %lld",
                    L(context, "Inventory", "sTotalItems", "Total Items"),
                    state.cachedInventory.size(),
                    L(context, "Inventory", "sTotalWeight", "Total Weight"),
                    totalWeight,
                    L(context, "Inventory", "sCarryWeight", "Carry Weight"),
                    totalWeight,
                    carryWeight,
                    L(context, "Inventory", "sCaps", "Caps"),
                    static_cast<long long>(context.inventory ? context.inventory->capsCount : 0));

                ImVec4 color = ImVec4(0.30f, 0.72f, 0.38f, 1.0f);
                if (ratio >= 0.90f) {
                    color = ImVec4(0.82f, 0.28f, 0.22f, 1.0f);
                } else if (ratio >= 0.70f) {
                    color = ImVec4(0.88f, 0.68f, 0.20f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                ImGui::ProgressBar((std::clamp)(ratio, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
                ImGui::PopStyleColor();
            }

            void DrawInventoryDetails(const InventoryEntry& entry, const InventoryTabView& context)
            {
                FormEntry formEntry{};
                formEntry.formID = entry.formID;
                formEntry.name = entry.name;
                formEntry.category = ResolveCategoryLabel(entry.category, context);
                formEntry.sourcePlugin = entry.sourcePlugin;

                requests.details = DetailKey{ entry.formID, context.session, context.catalog->generation, context.advancedDetails };
                FormDetailsViewContext detailsContext{
                    .localize = context.localize,
                    .showAdvancedDetailsView = context.advancedDetails,
                    .catalog = context.catalog.get(),
                    .details = context.details && context.details->key == *requests.details ? context.details : nullptr
                };
                FormDetailsView::Draw(formEntry, detailsContext);

                if (entry.stacks.size() > 1) {
                    const auto chosen = SelectedInstance(entry);
                    const auto preview = chosen ? std::string(L(context, "Inventory", "sInstance", "Instance")) + " " + std::to_string(1 + (std::ranges::find(entry.stacks, *chosen) - entry.stacks.begin())) :
                        std::string(L(context, "Inventory", "sChooseInstance", "Choose an instance"));
                    if (ImGui::BeginCombo((std::string(L(context, "Inventory", "sInstance", "Instance")) + "###InventoryInstance").c_str(), preview.c_str())) {
                        for (std::size_t position = 0; position < entry.stacks.size(); ++position) {
                            const auto index = entry.stacks[position];
                            const auto& stack = entry.source->stacks[index];
                            const auto label = std::string(L(context, "Inventory", "sInstance", "Instance")) + " " + std::to_string(position + 1) + " (" +
                                L(context, "Inventory", "sQuantity", "Qty") + ": " + std::to_string(stack.count) + ", " +
                                L(context, "Inventory", "sMods", "Mods") + ": " + std::to_string(stack.modCount) + ")###" + std::to_string(stack.token);
                            if (ImGui::Selectable(label.c_str(), chosen && *chosen == index)) state.instanceChoices[entry.groupID] = stack.token;
                        }
                        ImGui::EndCombo();
                    }
                    if (!SelectedInstance(entry)) ImGui::TextWrapped("%s", L(context, "Inventory", "sInstanceRequired", "Choose an instance to equip or use. Group removal affects all captured stacks."));
                }
                const auto selected = SelectedInstance(entry);
                const auto& instance = entry.source->stacks[selected.value_or(entry.representative)];
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (entry.stacks.size() > 1) ImGui::Text("%s: %llu", L(context, "Inventory", "sGroupQuantity", "Group Quantity"), static_cast<unsigned long long>(entry.count));
                ImGui::Text("%s: %llu", L(context, "Inventory", "sQuantity", "Qty"), static_cast<unsigned long long>(instance.count));
                ImGui::Text("%s: %s", L(context, "Inventory", "sEquipped", "Equipped"), instance.isEquipped ? L(context, "General", "sYes", "Yes") : L(context, "General", "sNo", "No"));
                ImGui::Text("%s: %s", L(context, "General", "sFavorite", "Favorite"), instance.isFavorited ? L(context, "General", "sYes", "Yes") : L(context, "General", "sNo", "No"));
                if (instance.isQuestItem) {
                    ImVec4 questColor = GetCategoryColor("", true, false);
                    ImGui::PushStyleColor(ImGuiCol_Text, questColor);
                    ImGui::Text("%s: %s", L(context, "Inventory", "sQuestItem", "Quest Item"), L(context, "General", "sYes", "Yes"));
                    ImGui::PopStyleColor();
                }
                ImGui::Text("%s: %.2f", L(context, "Inventory", "sWeight", "Weight"), instance.weight);
                ImGui::Text("%s: %d", L(context, "Inventory", "sValue", "Value"), instance.value);
                if (IsWeaponCategory(entry.category)) {
                    ImGui::Text("%s: %u", L(context, "Inventory", "sDamageOrRating", "DMG/DR"), instance.damage);
                } else if (IsArmorCategory(entry.category)) {
                    ImGui::Text("%s: %u", L(context, "Inventory", "sDamageOrRating", "DMG/DR"), instance.armorRating);
                }

                if (!instance.legendaryName.empty()) {
                    ImGui::Text("%s: %s", L(context, "Inventory", "sLegendary", "Legendary"), instance.legendaryName.c_str());
                }
                if (instance.healthPercent >= 0.0f) {
                    ImGui::Text("%s: %.0f%%", L(context, "Inventory", "sCondition", "Condition"), instance.healthPercent * 100.0f);
                }

                ImGui::Spacing();
                ImGui::TextUnformatted(L(context, "Inventory", "sCurrentMods", "Current Mods"));
                if (instance.mods.empty()) {
                    ImGui::TextDisabled("%s", L(context, "Inventory", "sNoMods", "No mods attached"));
                } else {
                    for (const auto& currentMod : instance.mods) {
                        if (currentMod.disabled) continue;
                        ImGui::BulletText("%s: %s", currentMod.slotLabel.c_str(), currentMod.name.c_str());
                    }
                }

                ImGui::Spacing();
                ImGui::TextUnformatted(L(context, "Inventory", "sEnchantments", "Enchantments"));
                if (instance.enchantments.empty()) {
                    ImGui::TextDisabled("%s", L(context, "General", "sNone", "None"));
                } else {
                    for (const auto& enchantment : instance.enchantments) {
                        ImGui::BulletText("%s", enchantment.name.c_str());
                    }
                }
            }

            void DrawCategoryTabs(const InventoryTabView& context)
            {
                const std::array<std::pair<InventoryCategoryTab, const char*>, 10> tabs{{
                    { InventoryCategoryTab::All, L(context, "Inventory", "sAllItems", "All") },
                    { InventoryCategoryTab::Weapons, L(context, "Inventory", "sWeapons", "Weapons") },
                    { InventoryCategoryTab::Armor, L(context, "Inventory", "sArmor", "Armor") },
                    { InventoryCategoryTab::Ammo, L(context, "Inventory", "sAmmo", "Ammo") },
                    { InventoryCategoryTab::Aid, L(context, "Inventory", "sAid", "Aid") },
                    { InventoryCategoryTab::Misc, L(context, "Inventory", "sMisc", "Misc") },
                    { InventoryCategoryTab::Keys, L(context, "Inventory", "sKeys", "Keys") },
                    { InventoryCategoryTab::Notes, L(context, "Inventory", "sNotes", "Notes/Holotapes") },
                    { InventoryCategoryTab::Components, L(context, "Inventory", "sComponents", "Components") },
                    { InventoryCategoryTab::Junk, L(context, "Inventory", "sJunk", "Junk") },
                }};

                if (!ImGui::BeginTabBar("InventoryCategories")) {
                    return;
                }

                for (const auto& [category, label] : tabs) {
                    if (ImGui::BeginTabItem(label)) {
                        state.activeCategory = category;
                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }

            std::vector<InventoryEntry> CopySelectedEntries(const std::vector<const InventoryEntry*>& visibleEntries)
            {
                std::vector<InventoryEntry> selectedEntries{};
                selectedEntries.reserve(state.selection.selected.size());
                for (const auto* entry : visibleEntries) {
                    if (state.selection.selected.contains(MakeRowKey(*entry))) {
                        selectedEntries.push_back(*entry);
                    }
                }
                return selectedEntries;
            }

            void DrawInventoryContextMenu(const InventoryEntry& entry, const InventoryTabView& context)
            {
                const bool gameplayActionsAllowed = context.gameplayReady;
                const std::string displayName = entry.name.empty() ? std::string(L(context, "General", "sUnnamed", "<Unnamed>")) : entry.name;

                ImGui::TextUnformatted(displayName.c_str());
                const std::string formIDText = FormatUtils::FormID(entry.formID);
                ImGui::TextDisabled("%s  |  %s", formIDText.c_str(), entry.sourcePlugin.c_str());
                ImGui::Separator();

                if (!gameplayActionsAllowed) {
                    ImGui::BeginDisabled(true);
                }

                if (ImGui::MenuItem(L(context, "Inventory", "sRemoveItem", "Remove Item"), nullptr, false, !entry.isQuestItem)) {
                    char message[512]{};
                    std::snprintf(message, sizeof(message), L(context, "Inventory", "sRemoveItemConfirm", "Remove %s (x%d) from inventory?"), displayName.c_str(), static_cast<int>((std::min)(entry.count, static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))));
                    ConfirmEntries(std::span{ &entry, 1 }, InventoryAction::Remove, L(context, "Inventory", "sRemoveItem", "Remove Item"), message);
                }

                if (ImGui::MenuItem(L(context, "Inventory", "sDropItem", "Drop Item"), nullptr, false, !entry.isQuestItem)) {
                    char message[512]{};
                    std::snprintf(message, sizeof(message), L(context, "Inventory", "sDropItemConfirm", "Drop %s (x%d)?"), displayName.c_str(), static_cast<int>((std::min)(entry.count, static_cast<std::uint64_t>((std::numeric_limits<int>::max)()))));
                    ConfirmEntries(std::span{ &entry, 1 }, InventoryAction::Drop, L(context, "Inventory", "sDropItem", "Drop Item"), message);
                }

                auto& desiredCount = state.desiredCounts.try_emplace(entry.groupID, 1).first->second;
                desiredCount = (std::clamp)(desiredCount, 0, static_cast<int>(ActionQueue::MaxQuantity));
                ImGui::SetNextItemWidth(140.0f);
                ImGui::InputInt("##SetCountInput", &desiredCount, 1, 10);
                desiredCount = (std::clamp)(desiredCount, 0, static_cast<int>(ActionQueue::MaxQuantity));
                const bool increase = static_cast<std::uint64_t>(desiredCount) > entry.count;
                const auto applyCountLabel = increase ? std::string(L(context, "Inventory", "sAddBaseItems", "Add Base Items")) + " (+" + std::to_string(static_cast<std::uint64_t>(desiredCount) - entry.count) + ")###ApplyInventoryCount" :
                    std::string(L(context, "Inventory", "sApplySetCount", "Apply Count")) + "###ApplyInventoryCount";
                if (ImGui::MenuItem(applyCountLabel.c_str())) {
                    AdjustInventoryEntryCount(entry, desiredCount);
                }
                if (static_cast<std::uint64_t>(desiredCount) > entry.count && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L(context, "Inventory", "sBaseItemNotice", "Adds the base item without this instance’s modifications."));

                if (IsEquippable(entry)) {
                    if (ImGui::MenuItem(entry.isEquipped ? L(context, "Inventory", "sUnequipItem", "Unequip") : L(context, "Inventory", "sEquipItem", "Equip"))) {
                        EquipInventoryEntry(entry, !entry.isEquipped);
                    }
                    if (ImGui::MenuItem(L(context, "Inventory", "sAddBaseItem", "Add Base Item"))) {
                        AddBaseItems(entry, 1);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L(context, "Inventory", "sBaseItemNotice", "Adds the base item without this instance’s modifications."));
                } else if (IsAidCategory(entry.category)) {
                    if (ImGui::MenuItem(L(context, "Inventory", "sUseItem", "Use"))) {
                        UseInventoryEntry(entry);
                    }
                }

                if (!gameplayActionsAllowed) {
                    ImGui::EndDisabled();
                }

                ImGui::Separator();

                if (ImGui::MenuItem(L(context, "General", "sCopyFormID", "Copy FormID"))) {
                    ImGui::SetClipboardText(FormatUtils::FormID(entry.formID).c_str());
                }
                if (ImGui::MenuItem(L(context, "General", "sCopyName", "Copy Name"))) {
                    ImGui::SetClipboardText(displayName.c_str());
                }
                if (!entry.editorID.empty()) {
                    if (ImGui::MenuItem(L(context, "General", "sCopyEditorID", "Copy EditorID"))) {
                        ImGui::SetClipboardText(entry.editorID.c_str());
                    }
                }
                if (ImGui::MenuItem(L(context, "General", "sCopyRecordSource", "Copy Record Source"))) {
                    ImGui::SetClipboardText(entry.sourcePlugin.c_str());
                }
                if (ImGui::MenuItem(L(context, "Inventory", "sInspectInBrowser", "Inspect in Plugin Browser"))) {
                    requests.inspect = entry.formID;
                }
            }

            void Draw(const InventoryTabView& context)
            {
                RefreshPlayerInventory(context);

                DrawQuickActions(context);
                SearchBar::Draw(L(context, "Inventory", "sSearch", "Search Inventory..."), state.inventorySearchBuffer.data(), state.inventorySearchBuffer.size(), state.inventorySearch, &state.focusPending, "InventorySearch", L(context, "General", "sClearSearchButton", "X"));
                ImGui::Checkbox(L(context, "Inventory", "sEquipped", "Equipped"), &state.showEquippedOnly);
                const auto* refreshLabel = L(context, "Inventory", "sRefreshInventory", "Refresh");
                ImGuiWidgetUtils::DrawWrappedSameLine(refreshLabel);
                if (ImGui::Button(refreshLabel)) {
                    MarkRefreshNeeded();
                    RefreshPlayerInventory(context);
                }

                if (!context.inventory || !context.inventory->ready) ImGui::TextDisabled("%s", L(context, "Inventory", "sLoading", "Loading inventory..."));
                if (state.selectionChanged) ImGui::TextWrapped("%s", L(context, "Inventory", "sSelectionChanged", "Inventory changed. Select the items again."));
                const auto feedback = InventoryFeedback::Message(state.rejection, context.localize);
                if (!feedback.empty()) ImGui::TextWrapped("%s", feedback.c_str());
                ActionFeedback::Draw(state.admission, context.localize);
                DrawSummaryBar(context);
                ImGui::Separator();
                DrawCategoryTabs(context);

                auto visibleEntries = BuildVisibleEntries();

                const ImVec2 available = ImGui::GetContentRegionAvail();
                const bool sideBySide = available.x >= 1020.0f;
                const ImVec2 tablePaneSize = sideBySide ? ImVec2(available.x * 0.64f, available.y) : ImVec2(0.0f, available.y * 0.58f);
                const ImVec2 detailPaneSize = sideBySide ? ImVec2(0.0f, available.y) : ImVec2(0.0f, 0.0f);

                const auto selectedEntries = CopySelectedEntries(visibleEntries);
                if (selectedEntries.size() > 1) {
                    bool firstButton = true;
                    if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sRemoveSelected", "Remove Selected"), firstButton)) {
                        ConfirmEntries(selectedEntries, InventoryAction::Remove, L(context, "Inventory", "sRemoveSelected", "Remove Selected"), L(context, "General", "sConfirmAction", "Confirm Action"));
                    }
                    if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sDropSelected", "Drop Selected"), firstButton)) {
                        ConfirmEntries(selectedEntries, InventoryAction::Drop, L(context, "Inventory", "sDropSelected", "Drop Selected"), L(context, "General", "sConfirmAction", "Confirm Action"));
                    }
                    if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sEquipSelected", "Equip Selected"), firstButton)) {
                        std::vector<InventoryEntry> equippable;
                        for (const auto& entry : selectedEntries) if (IsEquippable(entry)) equippable.push_back(entry);
                        SubmitPrepared(PrepareEntries(equippable, InventoryAction::Equip));
                    }
                    if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sUnequipSelected", "Unequip Selected"), firstButton)) {
                        std::vector<InventoryEntry> equippable;
                        for (const auto& entry : selectedEntries) if (IsEquippable(entry)) equippable.push_back(entry);
                        SubmitPrepared(PrepareEntries(equippable, InventoryAction::Unequip));
                    }
                    ImGui::Separator();
                }

                if (ImGui::BeginChild("InventoryTablePane", tablePaneSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                    const float tableHeight = (std::max)(220.0f, ImGui::GetContentRegionAvail().y);
                    if (ImGui::BeginTable("InventoryTable", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sName", "Name"), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sCategory", "Category"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sQuantity", "Qty"), ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sValue", "Value"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sWeight", "Weight"), ImGuiTableColumnFlags_WidthFixed, 65.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sDamageOrRating", "DMG/DR"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sMods", "Mods"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sSource", "Source"), ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();

                        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
                            SortVisibleEntries(visibleEntries, sortSpecs);
                            sortSpecs->SpecsDirty = false;
                        } else {
                            SortVisibleEntries(visibleEntries, nullptr);
                        }

                        std::vector<std::uint64_t> displayOrder;
                        displayOrder.reserve(visibleEntries.size());
                        for (const auto* entry : visibleEntries) displayOrder.push_back(entry->groupID);
                        state.selection.Reconcile(displayOrder);
                        ImGuiListClipper clipper;
                        clipper.Begin(static_cast<int>(visibleEntries.size()));
                        while (clipper.Step()) {
                            for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                                const auto& entry = *visibleEntries[static_cast<std::size_t>(rowIndex)];
                                const std::uint64_t rowKey = MakeRowKey(entry);
                                const bool rowSelected = state.selection.selected.contains(rowKey);

                                ImGui::PushID(static_cast<int>(entry.formID));
                                ImGui::PushID(std::to_string(entry.groupID).c_str());
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);

                                const ImVec4 rowColor = GetCategoryColor(entry.category, entry.isQuestItem, entry.isEquipped);
                                ImGui::PushStyleColor(ImGuiCol_Text, rowColor);

                                std::string rowLabel = entry.name.empty() ? std::string(L(context, "General", "sUnnamed", "<Unnamed>")) : entry.name;
                                if (entry.isEquipped) {
                                    rowLabel += " (";
                                    rowLabel += L(context, "Inventory", "sEquipped", "Equipped");
                                    rowLabel += ")";
                                }
                                const bool clicked = ImGui::Selectable((rowLabel + "###InventoryRow").c_str(), rowSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
                                if (clicked) {
                                    state.selection.Click(displayOrder, rowKey, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
                                    state.selectionChanged = false;
                                    state.rejection = InventoryRejection::None;

                                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                        if (IsEquippable(entry)) {
                                            EquipInventoryEntry(entry, !entry.isEquipped);
                                        } else if (IsAidCategory(entry.category)) {
                                            UseInventoryEntry(entry);
                                        }
                                    }
                                }

                                SharedUtils::DrawCurrentItemChrome(rowSelected, ImGui::IsItemHovered(), false, true);
                                if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !rowSelected) {
                                    state.selection.Single(rowKey);
                                    state.selectionChanged = false;
                                    state.rejection = InventoryRejection::None;
                                }
                                ImGui::OpenPopupOnItemClick("InventoryRowContext", ImGuiPopupFlags_MouseButtonRight);

                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextUnformatted(ResolveCategoryLabel(entry.category, context).c_str());
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%llu", static_cast<unsigned long long>(entry.count));
                                ImGui::TableSetColumnIndex(3);
                                ImGui::Text("%d", entry.value);
                                ImGui::TableSetColumnIndex(4);
                                ImGui::Text("%.2f", entry.weight);
                                ImGui::TableSetColumnIndex(5);
                                ImGui::Text("%u", IsArmorCategory(entry.category) ? entry.armorRating : entry.damage);
                                ImGui::TableSetColumnIndex(6);
                                ImGui::Text("%u", entry.modCount);
                                ImGui::TableSetColumnIndex(7);
                                ImGui::TextUnformatted(entry.sourcePlugin.c_str());

                                ImGui::PopStyleColor();

                        if (ImGui::BeginPopup("InventoryRowContext")) {
                            if (state.selectionChanged) ImGui::CloseCurrentPopup();
                            else DrawInventoryContextMenu(entry, context);
                                    ImGui::EndPopup();
                                }

                                ImGui::PopID();
                                ImGui::PopID();
                            }
                        }

                        ImGui::EndTable();
                    }
                }
                ImGui::EndChild();

                if (sideBySide) {
                    ImGui::SameLine();
                }

                const InventoryEntry* selectedEntry = nullptr;
                for (const auto* candidate : visibleEntries) {
                    if (candidate->groupID == state.selection.active && state.selection.selected.contains(candidate->groupID)) { selectedEntry = candidate; break; }
                }
                if (!selectedEntry) {
                    for (const auto* entry : visibleEntries) {
                        if (state.selection.selected.contains(MakeRowKey(*entry))) {
                            selectedEntry = entry;
                            break;
                        }
                    }
                }

                if (ImGui::BeginChild("InventoryDetailPane", detailPaneSize, sideBySide)) {
                    if (selectedEntry) {
                        DrawInventoryDetails(*selectedEntry, context);
                    } else {
                        ImGui::TextDisabled("%s", L(context, "Inventory", "sSelectItemHint", "Select an inventory item to view details."));
                    }
                }
                ImGui::EndChild();

                if (!ImGui::IsAnyItemActive() && !visibleEntries.empty()) {
                    if (!selectedEntries.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                        ConfirmEntries(selectedEntries, InventoryAction::Remove, L(context, "Inventory", "sRemoveSelected", "Remove Selected"), L(context, "General", "sConfirmAction", "Confirm Action"));
                    }

                    if (selectedEntry && ImGui::IsKeyPressed(ImGuiKey_E, false) && IsEquippable(*selectedEntry)) {
                        EquipInventoryEntry(*selectedEntry, !selectedEntry->isEquipped);
                    }
                }
            }

        };
    }

    void InventoryTab::Draw(InventoryTabState& state, const InventoryTabView& context, InventoryTabRequests& requests)
    {
        const ProfileScope profileScope(ProfileMetric::InventoryView);
        if (!context.catalog) return;
        InventoryView(state, context, requests).Draw(context);
    }

    void InventoryTab::ResetState(InventoryTabState& state) { const auto quick = state.quick; state = {}; state.quick = quick; }
}
