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
#include <imgui_internal.h>


#include <algorithm>
#include <array>
#include <cstdio>
#include <format>
#include <limits>
#include <map>
#include <ranges>
#include <unordered_map>

namespace ESPExplorerAE
{
    namespace
    {
        // ImGui borrows localization pointers. Restore its previous entries
        // before the view's language-frame owner releases them.
        class InventoryTableText
        {
            std::array<ImGuiLocEntry, 4> previous;
        public:
            explicit InventoryTableText(const std::array<ImGuiLocEntry, 4>& entries)
            {
                for (std::size_t index = 0; index < entries.size(); ++index) previous[index] = { entries[index].Key, ImGui::LocalizeGetMsg(entries[index].Key) };
                ImGui::LocalizeRegisterEntries(entries.data(), static_cast<int>(entries.size()));
            }
            ~InventoryTableText() { ImGui::LocalizeRegisterEntries(previous.data(), static_cast<int>(previous.size())); }
        };

        class InventoryView
        {
            InventoryTabState& state;
            const InventoryTabView& view;
            InventoryTabRequests& requests;
            bool inventoryChanged{};
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
                if (category == "BOOK" || category == "NOTE") {
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
                    inventoryChanged = true;
                    std::uint64_t retainedToken{};
                    // Retain a single chosen stack across count changes or new
                    // copies, never choose among different same-name instances.
                    // Bulk selections and pending quantity edits still expire.
                    if (state.snapshot && next && next->ready && state.snapshot->session == next->session &&
                        next->generation >= state.snapshot->generation && state.selection.selected.size() == 1) {
                        for (const auto& entry : state.cachedInventory) {
                            if (!state.selection.selected.contains(entry.groupID)) continue;
                            if (const auto chosen = SelectedInstance(entry)) {
                                auto previous = entry.source->stacks[*chosen];
                                if (const auto* current = next->Find(previous.token)) {
                                    previous.count = current->count;
                                    if (previous == *current) retainedToken = current->token;
                                }
                            }
                            break;
                        }
                    }
                    state.selectionChanged = !state.selection.selected.empty();
                    state.selection.Clear();
                    state.instanceChoices.clear();
                    state.desiredCounts.clear();
                    if (retainedToken) {
                        for (const auto& group : next->groups) {
                            if (std::ranges::none_of(group.stacks, [&](InventoryIndex index) { return next->stacks[index].token == retainedToken; })) continue;
                            state.selection.Single(group.id);
                            state.instanceChoices[group.id] = retainedToken;
                            state.selectionChanged = false;
                            break;
                        }
                    }
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
                    entry.totalWeight = group.totalWeight;
                    entry.totalValue = group.totalValue;
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
                    if (state.showFavoritesOnly && !entry.isFavorited) continue;
                    if (state.showLegendaryOnly && !entry.isLegendary) continue;
                    if (state.showQuestOnly && !entry.isQuestItem) continue;
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
                case 8:
                    return left.totalWeight < right.totalWeight ? -1 : (left.totalWeight > right.totalWeight ? 1 : 0);
                case 9:
                    return left.totalValue < right.totalValue ? -1 : (left.totalValue > right.totalValue ? 1 : 0);
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

            bool SelectedInstanceEquipped(const InventoryEntry& entry)
            {
                const auto selected = SelectedInstance(entry);
                return selected && entry.source->stacks[*selected].isEquipped;
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
                if (inventoryChanged) return false;
                state.rejection = prepared.rejection;
                if (!prepared) return false;
                if (prepared.plan.source->session != view.session) { state.admission = ActionAdmission::StaleSession; return false; }
                if (!view.gameplayReady) { state.admission = ActionAdmission::Unavailable; return false; }
                for (auto request : InventoryRequests(prepared)) requests.records.actions.push_back({ std::move(request), false, {} });
                return true;
            }

            void ConfirmEntries(std::span<const InventoryEntry> entries, InventoryAction action, std::string title, std::string message)
            {
                if (inventoryChanged) return;
                const auto prepared = PrepareEntries(entries, action);
                state.rejection = prepared.rejection;
                if (!prepared) return;
                if (prepared.plan.source->session != view.session) { state.admission = ActionAdmission::StaleSession; return; }
                if (!view.gameplayReady) { state.admission = ActionAdmission::Unavailable; return; }
                if (entries.size() > 1) {
                    std::uint64_t count{};
                    for (const auto& entry : entries) count += entry.count;
                    message += "\n" + std::string(L(view, "Inventory", "sItemGroups", "Item Groups")) + ": " + std::to_string(entries.size()) +
                        "  |  " + L(view, "Inventory", "sQuantity", "Qty") + ": " + std::to_string(count);
                    for (const auto& entry : entries.first((std::min)(entries.size(), std::size_t{ 6 }))) {
                        message += "\n" + (entry.name.empty() ? std::string(L(view, "General", "sUnnamed", "<Unnamed>")) : entry.name) + " (x" + std::to_string(entry.count) + ")";
                    }
                    if (entries.size() > 6) message += "\n...";
                }
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

            bool DuplicateItem(const InventoryEntry& entry)
            {
                return SubmitPrepared(PrepareEntries(std::span{ &entry, 1 }, InventoryAction::DuplicateItem));
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
                wrappedSameLine(noonLabel);
                if (ImGui::Button(noonLabel)) { state.quick.gameHour = 12.0f; }
                wrappedSameLine(eveningLabel);
                if (ImGui::Button(eveningLabel)) { state.quick.gameHour = 18.0f; }
                wrappedSameLine(midnightLabel);
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
                if (!context.inventory || !context.inventory->ready) return;
                const float totalWeight = CalculateTotalWeight();
                const float carryWeight = context.inventory ? context.inventory->carryWeight : 0.0f;
                const float ratio = carryWeight > 0.0f ? totalWeight / carryWeight : 0.0f;

                ImGui::TextWrapped(
                    "%s: %zu  |  %s: %.1f / %.1f  |  %s: %llu",
                    L(context, "Inventory", "sItemGroups", "Item Groups"),
                    state.cachedInventory.size(),
                    L(context, "Inventory", "sCarryWeight", "Carry Weight"),
                    totalWeight,
                    carryWeight,
                    L(context, "Inventory", "sCaps", "Caps"),
                    static_cast<unsigned long long>(context.inventory->capsCount));

                ImVec4 color = ImVec4(0.30f, 0.72f, 0.38f, 1.0f);
                if (ratio >= 0.90f) {
                    color = ImVec4(0.82f, 0.28f, 0.22f, 1.0f);
                } else if (ratio >= 0.70f) {
                    color = ImVec4(0.88f, 0.68f, 0.20f, 1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                ImGui::ProgressBar((std::clamp)(ratio, 0.0f, 1.0f), ImVec2(-1.0f, ImGui::GetFontSize() * 0.3f), "");
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L(context, "Inventory", "sTotalsHelp", "Totals include every instance in a row. Captured item weights may differ from Pip-Boy carry weight; values are not vendor prices."));
            }

            void DrawInstanceSelector(const InventoryEntry& entry, const InventoryTabView& context)
            {
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
                                L(context, "Inventory", "sMods", "Mods") + ": " + std::to_string(stack.modCount) + ")" +
                                (stack.isEquipped ? " (" + std::string(L(context, "Inventory", "sEquipped", "Equipped")) + ")" : "") + "###" + std::to_string(stack.token);
                            if (ImGui::Selectable(label.c_str(), chosen && *chosen == index)) state.instanceChoices[entry.groupID] = stack.token;
                        }
                        ImGui::EndCombo();
                    }
                    if (!SelectedInstance(entry)) ImGui::TextWrapped("%s", L(context, "Inventory", "sInstanceRequired", "Choose an instance to equip, use, or duplicate. Group removal affects all captured stacks."));
                }
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
                ImGui::TextWrapped("%s", entry.name.empty() ? L(context, "General", "sUnnamed", "<Unnamed>") : entry.name.c_str());

                DrawInstanceSelector(entry, context);
                const auto selected = SelectedInstance(entry);
                const auto& instance = entry.source->stacks[selected.value_or(entry.representative)];
                bool firstAction = true;
                ImGui::BeginDisabled(inventoryChanged || !context.gameplayReady || !selected);
                if (IsEquippable(entry)) {
                    if (ImGuiWidgetUtils::DrawWrappedButton(instance.isEquipped ? L(context, "Inventory", "sUnequipItem", "Unequip") : L(context, "Inventory", "sEquipItem", "Equip"), firstAction)) {
                        EquipInventoryEntry(entry, !instance.isEquipped);
                    }
                } else if (IsAidCategory(entry.category)) {
                    if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sUseItem", "Use"), firstAction)) UseInventoryEntry(entry);
                }
                ImGui::BeginDisabled(!InventoryActionAllowed(instance, InventoryAction::DuplicateItem));
                if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sDuplicateItem", "Duplicate Item"), firstAction)) DuplicateItem(entry);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", L(context, "Inventory", "sDuplicateItemHelp", "Adds one copy of the chosen item with its attachments, legendary effects, name, condition, and instance stats. The copy is not equipped or favorited."));
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "General", "sActions", "Actions"), firstAction)) ImGui::OpenPopup("InventoryDetailActions");
                if (ImGui::BeginPopup("InventoryDetailActions")) {
                    if (inventoryChanged || state.selectionChanged) ImGui::CloseCurrentPopup();
                    else DrawInventoryContextMenu(entry, context);
                    ImGui::EndPopup();
                }
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (entry.stacks.size() > 1) ImGui::Text("%s: %llu", L(context, "Inventory", "sGroupQuantity", "Group Quantity"), static_cast<unsigned long long>(entry.count));
                ImGui::TextWrapped("%s: %.2f  |  %s: %lld", L(context, "Inventory", "sStackWeight", "Stack Weight"), entry.totalWeight,
                    L(context, "Inventory", "sStackValue", "Stack Value"), static_cast<long long>(entry.totalValue));
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
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(L(context, "Inventory", "sBaseDetails", "Base Record Details"))) FormDetailsView::Draw(formEntry, detailsContext);
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
                    if (ImGui::BeginTabItem(label, nullptr, state.resetCategory && category == InventoryCategoryTab::All ? ImGuiTabItemFlags_SetSelected : 0)) {
                        state.activeCategory = category;
                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
                state.resetCategory = false;
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
                DrawInstanceSelector(entry, context);

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

                auto& desiredCount = state.desiredCounts.try_emplace(entry.groupID, static_cast<int>((std::min)(entry.count, static_cast<std::uint64_t>(ActionQueue::MaxQuantity)))).first->second;
                desiredCount = (std::clamp)(desiredCount, 0, static_cast<int>(ActionQueue::MaxQuantity));
                ImGui::TextUnformatted(L(context, "Inventory", "sSetCount", "Set Count"));
                ImGui::SetNextItemWidth(140.0f);
                ImGui::InputInt("##SetCountInput", &desiredCount, 1, 10);
                desiredCount = (std::clamp)(desiredCount, 0, static_cast<int>(ActionQueue::MaxQuantity));
                const bool increase = static_cast<std::uint64_t>(desiredCount) > entry.count;
                const auto applyCountLabel = increase ? std::string(L(context, "Inventory", "sAddBaseItems", "Add Base Items")) + " (+" + std::to_string(static_cast<std::uint64_t>(desiredCount) - entry.count) + ")###ApplyInventoryCount" :
                    std::string(L(context, "Inventory", "sApplySetCount", "Apply Count")) + "###ApplyInventoryCount";
                const bool countChangeAllowed = static_cast<std::uint64_t>(desiredCount) != entry.count && (increase || !entry.isQuestItem);
                if (ImGui::MenuItem(applyCountLabel.c_str(), nullptr, false, countChangeAllowed)) {
                    AdjustInventoryEntryCount(entry, desiredCount);
                }
                if (static_cast<std::uint64_t>(desiredCount) > entry.count && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L(context, "Inventory", "sBaseItemNotice", "Adds the base item without this instance’s modifications."));

                if (IsEquippable(entry)) {
                    const bool equipped = SelectedInstanceEquipped(entry);
                    if (ImGui::MenuItem(equipped ? L(context, "Inventory", "sUnequipItem", "Unequip") : L(context, "Inventory", "sEquipItem", "Equip"), nullptr, false, SelectedInstance(entry).has_value())) {
                        EquipInventoryEntry(entry, !equipped);
                    }
                    if (ImGui::MenuItem(L(context, "Inventory", "sAddBaseItem", "Add Base Item"))) {
                        AddBaseItems(entry, 1);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L(context, "Inventory", "sBaseItemNotice", "Adds the base item without this instance’s modifications."));
                } else if (IsAidCategory(entry.category)) {
                    if (ImGui::MenuItem(L(context, "Inventory", "sUseItem", "Use"), nullptr, false, SelectedInstance(entry).has_value())) {
                        UseInventoryEntry(entry);
                    }
                }

                const auto selected = SelectedInstance(entry);
                const bool canDuplicate = selected && InventoryActionAllowed(entry.source->stacks[*selected], InventoryAction::DuplicateItem);
                if (ImGui::MenuItem(L(context, "Inventory", "sDuplicateItem", "Duplicate Item"), nullptr, false, canDuplicate)) DuplicateItem(entry);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", L(context, "Inventory", "sDuplicateItemHelp", "Adds one copy of the chosen item with its attachments, legendary effects, name, condition, and instance stats. The copy is not equipped or favorited."));

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

            void DrawFilters(const InventoryTabView& context)
            {
                const auto* refreshLabel = L(context, "Inventory", "sRefreshInventory", "Refresh");
                const float refreshWidth = ImGui::CalcTextSize(refreshLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
                const float searchWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x - refreshWidth - ImGui::GetStyle().ItemSpacing.x);
                SearchBar::Draw(L(context, "Inventory", "sSearch", "Search Inventory..."), state.inventorySearchBuffer.data(), state.inventorySearchBuffer.size(), state.inventorySearch, &state.focusPending, "InventorySearch", L(context, "General", "sClearSearchButton", "X"), searchWidth);
                ImGuiWidgetUtils::SameLineIfFits(refreshWidth);
                if (ImGui::Button(refreshLabel)) MarkRefreshNeeded();
                ImGui::Checkbox(L(context, "Inventory", "sEquipped", "Equipped"), &state.showEquippedOnly);
                const auto filter = [&](const char* label, bool& enabled) {
                    ImGuiWidgetUtils::SameLineIfFits(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x);
                    ImGui::Checkbox(label, &enabled);
                };
                filter(L(context, "Inventory", "sPipboyFavorites", "Pip-Boy Favorites"), state.showFavoritesOnly);
                filter(L(context, "Inventory", "sLegendary", "Legendary"), state.showLegendaryOnly);
                filter(L(context, "Inventory", "sQuestItem", "Quest Item"), state.showQuestOnly);
                const auto* resetLabel = L(context, "General", "sResetFilters", "Reset Filters");
                ImGuiWidgetUtils::DrawWrappedSameLine(resetLabel);
                if (ImGui::Button(resetLabel)) {
                    InventoryTab::ResetFilters(state);
                    state.focusPending = true;
                }
            }

            void Draw(const InventoryTabView& context)
            {
                const InventoryTableText tableText({{
                    { ImGuiLocKey_TableSizeOne, L(context, "Inventory", "sFitColumn", "Size Column to Fit") },
                    { ImGuiLocKey_TableSizeAllFit, L(context, "Inventory", "sFitAllColumns", "Size All Columns to Fit") },
                    { ImGuiLocKey_TableSizeAllDefault, L(context, "Inventory", "sResetColumnWidths", "Reset Column Widths") },
                    { ImGuiLocKey_TableResetOrder, L(context, "Inventory", "sResetColumnOrder", "Reset Column Order") }
                }});
                RefreshPlayerInventory(context);

                DrawQuickActions(context);
                DrawFilters(context);

                if (!context.gameplayReady) ImGui::TextDisabled("%s", L(context, "Inventory", "sUnavailable", "Inventory is unavailable."));
                else if (!context.inventory || !context.inventory->ready) ImGui::TextDisabled("%s", L(context, "Inventory", "sLoading", "Loading inventory..."));
                DrawSummaryBar(context);
                ImGui::Separator();
                DrawCategoryTabs(context);

                auto visibleEntries = BuildVisibleEntries();
                std::vector<std::uint64_t> visibleOrder;
                visibleOrder.reserve(visibleEntries.size());
                for (const auto* entry : visibleEntries) visibleOrder.push_back(entry->groupID);
                state.selection.Reconcile(visibleOrder);
                bool firstSelectionButton = true;
                ImGui::BeginDisabled(visibleEntries.empty());
                if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sSelectVisible", "Select Visible"), firstSelectionButton)) {
                    state.selection.All(visibleOrder);
                    state.selectionChanged = false;
                    state.rejection = InventoryRejection::None;
                }
                ImGui::EndDisabled();
                ImGui::BeginDisabled(state.selection.selected.empty());
                if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "General", "sClearSelection", "Clear Selection"), firstSelectionButton)) state.selection.Clear();
                ImGui::EndDisabled();
                const auto* selectionHelp = L(context, "Inventory", "sSelectionHelp", "Ctrl+click toggles items; Shift+click selects a range. Right-click a column header to choose columns.");
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", selectionHelp);
                const auto selectedEntries = CopySelectedEntries(visibleEntries);
                // Keep the toolbar and summary height independent of selection so
                // rows stay under the pointer during clicks and inventory refreshes.
                ImGui::BeginDisabled(selectedEntries.size() < 2);
                const auto actionsLabel = std::string(L(context, "General", "sActions", "Actions")) + "###InventorySelectionActions";
                if (ImGuiWidgetUtils::DrawWrappedButton(actionsLabel.c_str(), firstSelectionButton)) ImGui::OpenPopup("InventorySelectionMenu");
                ImGui::EndDisabled();
                std::uint64_t selectedCount{};
                double selectedWeight{};
                for (const auto& entry : selectedEntries) {
                    selectedCount += entry.count;
                    if (std::isfinite(entry.totalWeight)) selectedWeight += entry.totalWeight;
                }
                const auto summary = std::format("{}: {} / {}  |  {}: {} ({}: {}, {}: {:.2f})",
                    L(context, "General", "sVisible", "Visible"), visibleEntries.size(), state.cachedInventory.size(), L(context, "General", "sSelected", "Selected"), selectedEntries.size(),
                    L(context, "Inventory", "sQuantity", "Qty"), selectedCount, L(context, "Inventory", "sStackWeight", "Stack Weight"), selectedWeight);
                const float summaryWidth = ImGui::GetContentRegionAvail().x;
                ImGui::TextUnformatted(summary.c_str());
                if (ImGui::GetItemRectSize().x > summaryWidth && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", summary.c_str());
                if (ImGui::BeginPopup("InventorySelectionMenu")) {
                    if (inventoryChanged || state.selectionChanged || selectedEntries.size() < 2) ImGui::CloseCurrentPopup();
                    ImGui::Text("%s: %zu", L(context, "General", "sSelected", "Selected"), selectedEntries.size());
                    ImGui::Separator();
                    ImGui::BeginDisabled(!context.gameplayReady);
                    const bool hasQuestItem = std::ranges::any_of(selectedEntries, [](const auto& entry) { return entry.isQuestItem; });
                    ImGui::BeginDisabled(hasQuestItem);
                    if (ImGui::MenuItem(L(context, "Inventory", "sRemoveSelected", "Remove Selected"))) {
                        ConfirmEntries(selectedEntries, InventoryAction::Remove, L(context, "Inventory", "sRemoveSelected", "Remove Selected"), L(context, "General", "sConfirmAction", "Confirm Action"));
                    }
                    if (ImGui::MenuItem(L(context, "Inventory", "sDropSelected", "Drop Selected"))) {
                        ConfirmEntries(selectedEntries, InventoryAction::Drop, L(context, "Inventory", "sDropSelected", "Drop Selected"), L(context, "General", "sConfirmAction", "Confirm Action"));
                    }
                    ImGui::EndDisabled();
                    if (hasQuestItem && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", L(context, "Inventory", "sQuestRestriction", "Quest items cannot be removed or dropped."));
                    const bool hasEquipment = std::ranges::any_of(selectedEntries, [&](const auto& entry) { return IsEquippable(entry); });
                    ImGui::BeginDisabled(!hasEquipment);
                    if (ImGui::MenuItem(L(context, "Inventory", "sEquipSelected", "Equip Selected"))) {
                        std::vector<InventoryEntry> equippable;
                        for (const auto& entry : selectedEntries) if (IsEquippable(entry)) equippable.push_back(entry);
                        SubmitPrepared(PrepareEntries(equippable, InventoryAction::Equip));
                    }
                    if (ImGui::MenuItem(L(context, "Inventory", "sUnequipSelected", "Unequip Selected"))) {
                        std::vector<InventoryEntry> equippable;
                        for (const auto& entry : selectedEntries) if (IsEquippable(entry)) equippable.push_back(entry);
                        SubmitPrepared(PrepareEntries(equippable, InventoryAction::Unequip));
                    }
                    ImGui::EndDisabled();
                    ImGui::EndDisabled();
                    ImGui::EndPopup();
                }

                const ImVec2 available = ImGui::GetContentRegionAvail();
                const float spacing = ImGui::GetStyle().ItemSpacing.y;
                const bool sideBySide = available.x >= ImGui::GetFontSize() * 45.0f;
                const float paneHeight = (std::max)(ImGui::GetFrameHeight() * 2.0f, available.y);
                const ImVec2 tablePaneSize = sideBySide ? ImVec2(available.x * 0.64f, paneHeight) : ImVec2(0.0f, (paneHeight - spacing) * 0.58f);
                const ImVec2 detailPaneSize = sideBySide ? ImVec2(0.0f, paneHeight) : ImVec2(0.0f, (paneHeight - spacing) * 0.42f);
                if (ImGui::BeginChild("InventoryTablePane", tablePaneSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                    if (visibleEntries.empty() && context.inventory && context.inventory->ready) {
                        ImGui::TextWrapped("%s", state.cachedInventory.empty() ? L(context, "Inventory", "sEmptyInventory", "Your inventory is empty.") :
                            L(context, "Inventory", "sNoMatches", "No items match these filters. Reset filters to see all items."));
                    }
                    const float tableHeight = (std::max)(1.0f, ImGui::GetContentRegionAvail().y);
                    if (ImGui::BeginTable("InventoryTable", 10, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                        ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX, ImVec2(0.0f, tableHeight))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        const float unit = ImGui::GetFontSize();
                        const auto columnWidth = [&](const char* label, float minimum) {
                            return (std::max)(unit * minimum, ImGui::CalcTextSize(label).x + unit + ImGui::GetStyle().CellPadding.x * 2.0f);
                        };
                        ImGui::TableSetupColumn(L(context, "Inventory", "sName", "Name"), ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoHide, unit * 12.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sCategory", "Category"), ImGuiTableColumnFlags_DefaultHide, unit * 7.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sQuantity", "Qty"), ImGuiTableColumnFlags_PreferSortDescending, columnWidth(L(context, "Inventory", "sQuantity", "Qty"), 2.5f));
                        ImGui::TableSetupColumn(L(context, "Inventory", "sValue", "Value"), ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultHide, columnWidth(L(context, "Inventory", "sValue", "Value"), 3.5f));
                        ImGui::TableSetupColumn(L(context, "Inventory", "sWeight", "Weight"), ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultHide, columnWidth(L(context, "Inventory", "sWeight", "Weight"), 3.5f));
                        ImGui::TableSetupColumn(L(context, "Inventory", "sDamageOrRating", "DMG/DR"), ImGuiTableColumnFlags_DefaultHide, unit * 4.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sMods", "Mods"), ImGuiTableColumnFlags_DefaultHide, unit * 3.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sSource", "Source"), ImGuiTableColumnFlags_DefaultHide, unit * 12.0f);
                        ImGui::TableSetupColumn(L(context, "Inventory", "sStackWeight", "Stack Weight"), ImGuiTableColumnFlags_PreferSortDescending, columnWidth(L(context, "Inventory", "sStackWeight", "Stack Weight"), 6.5f));
                        ImGui::TableSetupColumn(L(context, "Inventory", "sStackValue", "Stack Value"), ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultHide, columnWidth(L(context, "Inventory", "sStackValue", "Stack Value"), 6.5f));
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
                                            EquipInventoryEntry(entry, !SelectedInstanceEquipped(entry));
                                        } else if (IsAidCategory(entry.category)) {
                                            UseInventoryEntry(entry);
                                        }
                                    }
                                }

                                SharedUtils::DrawCurrentItemChrome(state.selection.selected.contains(rowKey), ImGui::IsItemHovered(), false, true);
                                if (ImGui::IsItemHovered()) {
                                    ImGui::BeginTooltip();
                                    ImGui::TextUnformatted(entry.name.c_str());
                                    ImGui::TextDisabled("%s  |  %s", FormatUtils::FormID(entry.formID).c_str(), entry.sourcePlugin.c_str());
                                    if (entry.isFavorited) ImGui::TextUnformatted(L(context, "General", "sFavorite", "Favorite"));
                                    if (entry.isLegendary) ImGui::TextUnformatted(L(context, "Inventory", "sLegendary", "Legendary"));
                                    if (entry.isQuestItem) ImGui::TextUnformatted(L(context, "Inventory", "sQuestItem", "Quest Item"));
                                    ImGui::TextUnformatted(selectionHelp);
                                    ImGui::EndTooltip();
                                }
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
                                ImGui::TableSetColumnIndex(8);
                                ImGui::Text("%.2f", entry.totalWeight);
                                ImGui::TableSetColumnIndex(9);
                                ImGui::Text("%lld", static_cast<long long>(entry.totalValue));

                                ImGui::PopStyleColor();

                                if (ImGui::BeginPopup("InventoryRowContext")) {
                                    if (inventoryChanged || state.selectionChanged) ImGui::CloseCurrentPopup();
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

                const auto detailGroup = selectedEntry ? selectedEntry->groupID : 0;
                const auto selectedInstance = selectedEntry ? SelectedInstance(*selectedEntry) : std::nullopt;
                const auto detailToken = selectedInstance ? selectedEntry->source->stacks[*selectedInstance].token : 0;
                const bool newFeedback = state.detailsAdmission != state.admission || state.detailsRejection != state.rejection;
                if (state.detailsGroup != detailGroup || state.detailsToken != detailToken || newFeedback) ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
                state.detailsGroup = detailGroup;
                state.detailsToken = detailToken;
                state.detailsAdmission = state.admission;
                state.detailsRejection = state.rejection;
                // Reserve the scrollbar from the first frame: changing detail
                // length must not rewrap controls a frame after selection.
                if (ImGui::BeginChild("InventoryDetailPane", detailPaneSize, sideBySide, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                    if (state.selectionChanged) ImGui::TextWrapped("%s", L(context, "Inventory", "sSelectionChanged", "Inventory changed. Select the items again."));
                    const auto feedback = InventoryFeedback::Message(state.rejection, context.localize);
                    if (!feedback.empty()) ImGui::TextWrapped("%s", feedback.c_str());
                    ActionFeedback::Draw(state.admission, context.localize);
                    if (selectedEntry) {
                        DrawInventoryDetails(*selectedEntry, context);
                    } else {
                        ImGui::TextDisabled("%s", L(context, "Inventory", "sSelectItemHint", "Select an inventory item to view details."));
                    }
                }
                ImGui::EndChild();

                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                    !ImGui::IsAnyItemActive() && !visibleEntries.empty()) {
                    if (!state.selection.selected.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                        ConfirmEntries(CopySelectedEntries(visibleEntries), InventoryAction::Remove, L(context, "Inventory", "sRemoveSelected", "Remove Selected"), L(context, "General", "sConfirmAction", "Confirm Action"));
                    }

                    if (selectedEntry && ImGui::IsKeyPressed(ImGuiKey_E, false) && IsEquippable(*selectedEntry)) {
                        EquipInventoryEntry(*selectedEntry, !SelectedInstanceEquipped(*selectedEntry));
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

    void InventoryTab::ResetFilters(InventoryTabState& state)
    {
        state.inventorySearch.clear();
        state.inventorySearchBuffer.fill(0);
        state.showEquippedOnly = state.showFavoritesOnly = state.showLegendaryOnly = state.showQuestOnly = false;
        state.activeCategory = InventoryCategoryTab::All;
        state.resetCategory = true;
    }

    void InventoryTab::ResetState(InventoryTabState& state) { const auto quick = state.quick; state = {}; state.quick = quick; }
}
