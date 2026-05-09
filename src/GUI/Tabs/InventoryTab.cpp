#include "GUI/Tabs/InventoryTab.h"

#include "Config/Config.h"
#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormDetailsView.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/MainWindowPopups.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"
#include "Logging/Logger.h"

#include <imgui.h>

#include <RE/A/ActorEquipManager.h>
#include <RE/A/ActorValue.h>
#include <RE/A/AlchemyItem.h>
#include <RE/B/BGSAttachParentArray.h>
#include <RE/B/BGSKeyword.h>
#include <RE/B/BGSMod.h>
#include <RE/B/BGSObjectInstanceExtra.h>
#include <RE/E/ExtraDataList.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESAmmo.h>
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESKey.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectBOOK.h>
#include <RE/T/TESObjectMISC.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWeightForm.h>

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
        enum class InventoryCategoryTab
        {
            All,
            Weapons,
            Armor,
            Ammo,
            Aid,
            Misc,
            Keys,
            Notes,
            Components,
            Junk
        };

        struct CurrentModEntry
        {
            std::uint32_t formID{ 0 };
            std::uint8_t attachIndex{ 0 };
            std::string slotLabel;
            std::string name;
        };

        struct InventoryDetailState
        {
            RE::BSTSmartPointer<RE::ExtraDataList> extra{};
            RE::TBO_InstanceData* instanceData{ nullptr };
            RE::TESBoundObject* object{ nullptr };
            std::vector<CurrentModEntry> currentMods{};
            std::vector<std::string> enchantments{};
            std::string legendaryName{};
            float healthPercent{ -1.0f };
        };

        std::vector<InventoryEntry> cachedInventory{};
        int lastRefreshFrame{ -30 };
        bool forceRefresh{ true };
        char inventorySearchBuffer[256]{};
        std::string inventorySearch{};
        InventoryCategoryTab activeCategory{ InventoryCategoryTab::All };
        bool showEquippedOnly{ false };
        std::unordered_set<std::uint64_t> selectedInventoryRows{};
        int lastClickedInventoryRow{ -1 };
        std::uint64_t detailCacheKey{ 0 };
        int detailCacheFrame{ -30 };
        InventoryDetailState cachedDetailState{};

        const char* L(InventoryTabContext& context, std::string_view section, std::string_view key, const char* fallback)
        {
            return context.localize ? context.localize(section, key, fallback) : fallback;
        }

        std::uint64_t MakeRowKey(std::uint32_t formID, std::uint32_t stackID)
        {
            return (static_cast<std::uint64_t>(formID) << 32) | stackID;
        }

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

        std::string ResolvePluginName(const RE::TESForm* form)
        {
            if (!form) {
                return {};
            }

            const auto* file = form->GetFile(0);
            if (!file) {
                return {};
            }

            const auto filename = file->GetFilename();
            return filename.empty() ? std::string{} : std::string(filename);
        }

        std::string ResolveFormLabel(const RE::TESForm* form)
        {
            if (!form) {
                return {};
            }

            const auto fullName = RE::TESFullName::GetFullName(*form);
            if (!fullName.empty()) {
                return std::string(fullName);
            }

            if (const char* editorID = form->GetFormEditorID(); editorID && editorID[0] != '\0') {
                return editorID;
            }

            return FormatUtils::FormID(form->GetFormID());
        }

        std::string ResolveName(RE::BGSInventoryItem& item, RE::ExtraDataList* extra, RE::TESBoundObject* object)
        {
            if (extra) {
                if (const char* displayName = item.GetDisplayFullName(extra); displayName && displayName[0] != '\0') {
                    return displayName;
                }
            }

            return ResolveFormLabel(object);
        }

        std::string ResolveCategoryCode(RE::TESBoundObject* object)
        {
            if (!object) {
                return {};
            }

            switch (object->GetFormType()) {
            case RE::ENUM_FORM_ID::kWEAP:
                return "WEAP";
            case RE::ENUM_FORM_ID::kARMO:
                return "ARMO";
            case RE::ENUM_FORM_ID::kAMMO:
                return "AMMO";
            case RE::ENUM_FORM_ID::kALCH:
                return "ALCH";
            case RE::ENUM_FORM_ID::kKEYM:
                return "KEYM";
            case RE::ENUM_FORM_ID::kBOOK:
                return "BOOK";
            case RE::ENUM_FORM_ID::kNOTE:
                return "NOTE";
            case RE::ENUM_FORM_ID::kMISC:
                if (const auto* misc = object->As<RE::TESObjectMISC>()) {
                    if (misc->IsLooseMod()) {
                        return "OMOD";
                    }
                    if (misc->componentData != nullptr) {
                        return misc->GetFormWeight() <= 0.0f ? "CMPO" : "JUNK";
                    }
                }
                return "MISC";
            default:
                break;
            }

            if (const char* formTypeString = object->GetFormTypeString()) {
                return formTypeString;
            }
            return {};
        }

        std::string ResolveCategoryLabel(std::string_view category, InventoryTabContext& context)
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

        RE::BGSKeyword* ResolveAttachPointKeyword(const RE::BGSTypedKeywordValue<RE::KeywordType::kAttachPoint>& value)
        {
            return RE::detail::BGSKeywordGetTypedKeywordByIndex(RE::KeywordType::kAttachPoint, value.keywordIndex);
        }

        std::string ResolveKeywordLabel(const RE::BGSKeyword* keyword)
        {
            if (!keyword) {
                return {};
            }

            if (const char* editorID = keyword->GetFormEditorID(); editorID && editorID[0] != '\0') {
                return editorID;
            }

            return FormatUtils::FormID(keyword->GetFormID());
        }

        const RE::BGSAttachParentArray* GetAttachParents(RE::TESBoundObject* object)
        {
            if (const auto* weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr) {
                return &weapon->attachParents;
            }
            if (const auto* armor = object ? object->As<RE::TESObjectARMO>() : nullptr) {
                return &armor->attachParents;
            }
            return nullptr;
        }

        std::string ResolveSlotLabel(RE::TESBoundObject* object, std::uint8_t attachIndex)
        {
            const auto* attachParents = GetAttachParents(object);
            if (!attachParents || attachIndex >= attachParents->size || attachParents->array == nullptr) {
                return {};
            }

            return ResolveKeywordLabel(ResolveAttachPointKeyword(attachParents->array[attachIndex]));
        }

        bool CollectStackState(const InventoryEntry& entry, InventoryDetailState& state)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return false;
            }

            RE::BSAutoReadLock lock{ player->inventoryList->rwLock };
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != entry.formID) {
                    continue;
                }

                if (auto* stack = item.GetStackByID(entry.stackID)) {
                    state.object = item.object;
                    state.extra = stack->extra;
                    state.instanceData = item.GetInstanceData(entry.stackID);
                    return true;
                }
                return false;
            }

            return false;
        }

        InventoryEntry BuildInventoryEntry(RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack, std::uint32_t stackID)
        {
            InventoryEntry entry{};
            entry.formID = item.object ? item.object->GetFormID() : 0;
            entry.stackID = stackID;
            entry.count = stack.GetCount();
            entry.isEquipped = stack.IsEquipped();
            entry.category = ResolveCategoryCode(item.object);
            entry.sourcePlugin = ResolvePluginName(item.object);

            auto* extra = stack.extra.get();
            entry.name = ResolveName(item, extra, item.object);

            RE::TBO_InstanceData* instanceData = item.GetInstanceData(stackID);
            if (item.object) {
                entry.weight = RE::TESWeightForm::GetFormWeight(item.object, instanceData);
                entry.value = static_cast<std::int32_t>(RE::TESValueForm::GetFormValue(item.object, instanceData));
            }

            if (const auto* weaponData = instanceData ? dynamic_cast<const RE::TESObjectWEAP::InstanceData*>(instanceData) : nullptr) {
                entry.damage = weaponData->attackDamage;
            } else if (const auto* weapon = item.object ? item.object->As<RE::TESObjectWEAP>() : nullptr) {
                entry.damage = weapon->weaponData.attackDamage;
            }

            if (const auto* armorData = instanceData ? dynamic_cast<const RE::TESObjectARMO::InstanceData*>(instanceData) : nullptr) {
                entry.armorRating = armorData->rating;
            } else if (const auto* armor = item.object ? item.object->As<RE::TESObjectARMO>() : nullptr) {
                entry.armorRating = armor->armorData.rating;
            }

            if (extra) {
                entry.isFavorited = extra->IsFavorite();
                entry.isLegendary = extra->GetLegendaryMod() != nullptr;
                if (auto* objectInstance = extra->GetByType<RE::BGSObjectInstanceExtra>()) {
                    entry.modCount = objectInstance->GetNumMods(false);
                }
            }

            entry.isQuestItem = item.IsQuestObject(stackID);

            return entry;
        }

        void RefreshPlayerInventory()
        {
            const int currentFrame = ImGui::GetFrameCount();
            if (!forceRefresh && currentFrame - lastRefreshFrame < 30) {
                return;
            }

            forceRefresh = false;
            lastRefreshFrame = currentFrame;
            cachedInventory.clear();

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->inventoryList) {
                return;
            }

            RE::BSAutoReadLock lock{ player->inventoryList->rwLock };
            for (auto& item : player->inventoryList->data) {
                if (!item.object) {
                    continue;
                }

                std::uint32_t stackID = 0;
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++stackID) {
                    if (stack->GetCount() <= 0) {
                        continue;
                    }

                    cachedInventory.push_back(BuildInventoryEntry(item, *stack, stackID));
                }
            }

            std::unordered_map<std::string, std::size_t> mergeIndex{};
            std::vector<InventoryEntry> merged{};
            merged.reserve(cachedInventory.size());

            for (auto& entry : cachedInventory) {
                std::string key = std::to_string(entry.formID) + "|" + entry.name;
                auto it = mergeIndex.find(key);
                if (it != mergeIndex.end()) {
                    auto& existing = merged[it->second];
                    existing.count += entry.count;
                    if (entry.isEquipped) {
                        existing.isEquipped = true;
                        existing.stackID = entry.stackID;
                    }
                    if (entry.isFavorited) existing.isFavorited = true;
                    if (entry.isLegendary) existing.isLegendary = true;
                    if (entry.isQuestItem) existing.isQuestItem = true;
                    if (entry.modCount > existing.modCount) existing.modCount = entry.modCount;
                } else {
                    mergeIndex[key] = merged.size();
                    merged.push_back(std::move(entry));
                }
            }

            cachedInventory = std::move(merged);
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
            visibleEntries.reserve(cachedInventory.size());

            for (const auto& entry : cachedInventory) {
                if (!MatchesCategory(entry, activeCategory)) {
                    continue;
                }
                if (showEquippedOnly && !entry.isEquipped) {
                    continue;
                }
                if (!inventorySearch.empty()) {
                    const std::string formIDText = FormatUtils::FormID(entry.formID);
                    if (!SharedUtils::ContainsCaseInsensitive(entry.name, inventorySearch) &&
                        !SharedUtils::ContainsCaseInsensitive(entry.sourcePlugin, inventorySearch) &&
                        !SharedUtils::ContainsCaseInsensitive(entry.category, inventorySearch) &&
                        !SharedUtils::ContainsCaseInsensitive(formIDText, inventorySearch)) {
                        const char* editorID = ContextMenu::TryGetEditorID(entry.formID);
                        if (!editorID || !SharedUtils::ContainsCaseInsensitive(editorID, inventorySearch)) {
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
                    return left->stackID < right->stackID;
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
                return left->stackID < right->stackID;
            });
        }

        void MarkRefreshNeeded()
        {
            forceRefresh = true;
        }

        bool RemoveInventoryEntry(const InventoryEntry& entry, std::int32_t count, bool drop)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* form = RE::TESForm::GetFormByID(entry.formID);
            auto* object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (!player || !object || count <= 0) {
                return false;
            }

            RE::TESObjectREFR::RemoveItemData data{ object, count };
            data.stackData.push_back(entry.stackID);
            if (drop) {
                data.reason = RE::ITEM_REMOVE_REASON::KDropping;
            }

            player->RemoveItem(data);
            Logger::Verbose(std::string(drop ? "Dropped inventory stack form=" : "Removed inventory stack form=") + std::to_string(entry.formID) + " stack=" + std::to_string(entry.stackID) + " count=" + std::to_string(count));
            MarkRefreshNeeded();
            return true;
        }

        bool AdjustInventoryEntryCount(const InventoryEntry& entry, std::int32_t desiredCount)
        {
            desiredCount = (std::max)(0, desiredCount);
            if (desiredCount == static_cast<std::int32_t>(entry.count)) {
                return true;
            }

            if (desiredCount < static_cast<std::int32_t>(entry.count)) {
                return RemoveInventoryEntry(entry, static_cast<std::int32_t>(entry.count) - desiredCount, false);
            }

            FormActions::GiveToPlayer(entry.formID, static_cast<std::uint32_t>(desiredCount - static_cast<std::int32_t>(entry.count)));
            MarkRefreshNeeded();
            return true;
        }

        bool EquipInventoryEntry(const InventoryEntry& entry, bool equip)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* equipManager = RE::ActorEquipManager::GetSingleton();
            if (!player || !equipManager) {
                return false;
            }

            InventoryDetailState state{};
            if (!CollectStackState(entry, state) || !state.object) {
                return false;
            }

            RE::BGSObjectInstance objectInstance{ state.object, state.instanceData };
            const bool result = equip ?
                equipManager->EquipObject(player, objectInstance, entry.stackID, 1, nullptr, false, true, true, true, false) :
                equipManager->UnequipObject(player, &objectInstance, 1, nullptr, entry.stackID, false, true, true, true, nullptr);

            if (result) {
                Logger::Verbose(std::string(equip ? "Equipped inventory stack form=" : "Unequipped inventory stack form=") + std::to_string(entry.formID) + " stack=" + std::to_string(entry.stackID));
                MarkRefreshNeeded();
            }

            return result;
        }

        bool UseInventoryEntry(const InventoryEntry& entry)
        {
            return EquipInventoryEntry(entry, true);
        }

        void DuplicateInventoryEntry(const InventoryEntry& entry)
        {
            FormActions::GiveToPlayer(entry.formID, 1);
            MarkRefreshNeeded();
        }

        std::vector<CurrentModEntry> GatherCurrentMods(const InventoryEntry& entry)
        {
            std::vector<CurrentModEntry> currentMods{};
            InventoryDetailState state{};
            if (!CollectStackState(entry, state) || !state.extra || !state.object) {
                return currentMods;
            }

            if (auto* objectInstance = state.extra->GetByType<RE::BGSObjectInstanceExtra>()) {
                for (const auto& modData : objectInstance->GetIndexData()) {
                    if (modData.disabled) {
                        continue;
                    }

                    auto* form = RE::TESForm::GetFormByID(modData.objectID);
                    auto* mod = form ? form->As<RE::BGSMod::Attachment::Mod>() : nullptr;
                    if (!mod) {
                        continue;
                    }

                    CurrentModEntry currentMod{};
                    currentMod.formID = mod->GetFormID();
                    currentMod.attachIndex = modData.index;
                    currentMod.slotLabel = ResolveSlotLabel(state.object, modData.index);
                    currentMod.name = ResolveFormLabel(static_cast<const RE::TESForm*>(mod));
                    currentMods.push_back(std::move(currentMod));
                }
            }

            return currentMods;
        }

        std::vector<std::string> GatherEnchantments(RE::TBO_InstanceData* instanceData)
        {
            std::vector<std::string> enchantments{};
            if (!instanceData) {
                return enchantments;
            }

            auto* enchantmentArray = instanceData->GetEnchantmentArray();
            if (!enchantmentArray) {
                return enchantments;
            }

            enchantments.reserve(enchantmentArray->size());
            for (auto* enchantment : *enchantmentArray) {
                if (enchantment) {
                    enchantments.push_back(ResolveFormLabel(enchantment));
                }
            }

            return enchantments;
        }

        InventoryDetailState BuildInventoryDetailState(const InventoryEntry& entry)
        {
            InventoryDetailState state{};
            if (!CollectStackState(entry, state)) {
                return state;
            }

            state.currentMods = GatherCurrentMods(entry);
            state.enchantments = GatherEnchantments(state.instanceData);

            if (state.extra) {
                state.healthPercent = state.extra->GetHealthPerc();
                if (auto* legendary = state.extra->GetLegendaryMod()) {
                    state.legendaryName = ResolveFormLabel(static_cast<const RE::TESForm*>(legendary));
                }
            }

            return state;
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
            float totalWeight = 0.0f;
            for (const auto& entry : cachedInventory) {
                totalWeight += entry.weight * static_cast<float>(entry.count);
            }
            return totalWeight;
        }

        std::int64_t GetCapsCount()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* form = RE::TESForm::GetFormByID(FormActions::kCapsFormID);
            auto* object = form ? form->As<RE::TESBoundObject>() : nullptr;
            return (player && object) ? player->GetInventoryObjectCount(object) : 0;
        }

        float GetCarryWeight()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* actorValue = RE::ActorValue::GetSingleton();
            return (player && actorValue && actorValue->carryWeight) ? player->GetActorValue(*actorValue->carryWeight) : 0.0f;
        }

        void DrawQuickActions(InventoryTabContext& context)
        {
            if (!ImGui::CollapsingHeader(L(context, "Inventory", "sQuickActions", "Quick Actions"))) {
                return;
            }

            auto wrappedSameLine = [](const char* nextLabel) {
                ImGuiWidgetUtils::DrawWrappedSameLine(nextLabel);
            };

            const bool gameplayActionsAllowed = FormActions::AreGameplayActionsAllowed();
            const char* disabledTooltip = L(context, "General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");

            if (!gameplayActionsAllowed) {
                ImGui::BeginDisabled(true);
            }

            SharedUtils::DrawSectionLabel(L(context, "Inventory", "sCharacterSection", "Character"));

            if (ImGui::Button(L(context, "Inventory", "sRefillHealth", "Refill Health"))) {
                FormActions::ExecuteConsoleCommand("player.resethealth");
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            const char* godModeLabel = context.playerGodModeEnabled ? L(context, "Inventory", "sGodModeOff", "Godmode: ON") : L(context, "Inventory", "sGodModeOn", "Godmode: OFF");
            wrappedSameLine(godModeLabel);
            if (ImGui::Button(godModeLabel)) {
                FormActions::SetPlayerGodModeEnabled(!context.playerGodModeEnabled);
                context.playerGodModeEnabled = FormActions::IsPlayerGodModeEnabled();
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            const char* noClipLabel = context.playerNoClipEnabled ? L(context, "Inventory", "sNoClipOff", "Noclip: ON") : L(context, "Inventory", "sNoClipOn", "Noclip: OFF");
            wrappedSameLine(noClipLabel);
            if (ImGui::Button(noClipLabel)) {
                FormActions::ExecuteConsoleCommand("tcl");
                context.playerNoClipEnabled = !context.playerNoClipEnabled;
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            const float inputWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x * 0.2f);

            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputInt(L(context, "Inventory", "sSetLevel", "Set Level"), &context.playerLevelAmount, 1, 10);
            context.playerLevelAmount = (std::clamp)(context.playerLevelAmount, 1, 65535);
            const char* applyLevelLabel = L(context, "Inventory", "sApplyLevel", "Apply Level");
            wrappedSameLine(applyLevelLabel);
            if (ImGui::Button(applyLevelLabel)) {
                char command[64]{};
                std::snprintf(command, sizeof(command), "player.setlevel %d", context.playerLevelAmount);
                FormActions::ExecuteConsoleCommand(command);
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputInt(L(context, "Inventory", "sAddPerkPoints", "Perk Points"), &context.playerPerkPointsAmount, 1, 5);
            context.playerPerkPointsAmount = (std::clamp)(context.playerPerkPointsAmount, 1, 999);
            const char* addPerkPointsLabel = L(context, "Inventory", "sAddPerkPointsBtn", "Add Perk Points");
            wrappedSameLine(addPerkPointsLabel);
            if (ImGui::Button(addPerkPointsLabel)) {
                char command[128]{};
                std::snprintf(command, sizeof(command), "cgf \"Game.AddPerkPoints\" %d", context.playerPerkPointsAmount);
                FormActions::ExecuteConsoleCommand(command);
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            SharedUtils::DrawSectionLabel(L(context, "Inventory", "sAmmunitionSection", "Ammunition"));

            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputInt(L(context, "Inventory", "sCurrentWeaponAmmo", "Current Weapon Ammo"), &context.playerCurrentWeaponAmmoAmount, 10, 100);
            context.playerCurrentWeaponAmmoAmount = (std::max)(1, context.playerCurrentWeaponAmmoAmount);
            const char* addCurrentAmmoLabel = L(context, "Inventory", "sAddCurrentAmmo", "Add Ammo For Held Weapon");
            wrappedSameLine(addCurrentAmmoLabel);
            if (ImGui::Button(addCurrentAmmoLabel)) {
                FormActions::AddAmmoForCurrentWeapon(static_cast<std::uint32_t>(context.playerCurrentWeaponAmmoAmount));
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputInt(L(context, "Inventory", "sAllAmmoCount", "All Ammo Count"), &context.playerAllAmmoAmount, 10, 100);
            context.playerAllAmmoAmount = (std::max)(1, context.playerAllAmmoAmount);
            const char* addAllAmmoLabel = L(context, "Inventory", "sAddAllAmmo", "Add All Ammo Types");
            wrappedSameLine(addAllAmmoLabel);
            if (ImGui::Button(addAllAmmoLabel)) {
                for (const auto& ammo : context.cache.ammo) {
                    FormActions::GiveToPlayer(ammo.formID, static_cast<std::uint32_t>(context.playerAllAmmoAmount));
                }
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            SharedUtils::DrawSectionLabel(L(context, "Inventory", "sQuickItemsSection", "Quick Items"));

            if (ImGui::Button(L(context, "Inventory", "sAddStimpak", "Add Stimpaks"))) {
                FormEntry entry{};
                entry.formID = FormActions::kStimpakFormID;
                entry.name = L(context, "Inventory", "sItemStimpak", "Stimpak");
                entry.category = "Aid";
                context.openItemGrantPopup(entry);
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            const char* lockpickLabel = L(context, "Inventory", "sAddLockpick", "Add Lockpicks");
            wrappedSameLine(lockpickLabel);
            if (ImGui::Button(lockpickLabel)) {
                FormEntry entry{};
                entry.formID = FormActions::kLockpickFormID;
                entry.name = L(context, "Inventory", "sItemLockpick", "Lockpick");
                entry.category = "Misc";
                context.openItemGrantPopup(entry);
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            const char* capsLabel = L(context, "Inventory", "sAddCaps", "Add Caps");
            wrappedSameLine(capsLabel);
            if (ImGui::Button(capsLabel)) {
                FormEntry entry{};
                entry.formID = FormActions::kCapsFormID;
                entry.name = L(context, "Inventory", "sCaps", "Caps");
                entry.category = "Misc";
                context.openItemGrantPopup(entry);
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            SharedUtils::DrawSectionLabel(L(context, "Inventory", "sTimeOfDaySection", "Time of Day"));

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
                int timeOfDayHour = static_cast<int>(context.playerTimeOfDay + 0.5f);
                if (ImGui::SliderInt(
                        L(context, "Inventory", "sTimeOfDaySlider", "Hour"),
                        &timeOfDayHour, 0, 23, "%d")) {
                    context.playerTimeOfDay = static_cast<float>(timeOfDayHour);
                }

            const char* morningLabel = L(context, "Inventory", "sTimeMorning", "Morning");
            const char* noonLabel = L(context, "Inventory", "sTimeNoon", "Noon");
            const char* eveningLabel = L(context, "Inventory", "sTimeEvening", "Evening");
            const char* midnightLabel = L(context, "Inventory", "sTimeMidnight", "Midnight");
            const char* applyTimeLabel = L(context, "Inventory", "sApplyTime", "Set Time");

            if (ImGui::Button(morningLabel)) { context.playerTimeOfDay = 6.0f; }
            ImGui::SameLine();
            if (ImGui::Button(noonLabel)) { context.playerTimeOfDay = 12.0f; }
            ImGui::SameLine();
            if (ImGui::Button(eveningLabel)) { context.playerTimeOfDay = 18.0f; }
            ImGui::SameLine();
            if (ImGui::Button(midnightLabel)) { context.playerTimeOfDay = 0.0f; }
            wrappedSameLine(applyTimeLabel);
            if (ImGui::Button(applyTimeLabel)) {
                char command[64]{};
                std::snprintf(command, sizeof(command), "set gamehour to %.2f", context.playerTimeOfDay);
                FormActions::ExecuteConsoleCommand(command);
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);

            if (!gameplayActionsAllowed) {
                ImGui::EndDisabled();
            }
        }

        void DrawSummaryBar(InventoryTabContext& context)
        {
            const float totalWeight = CalculateTotalWeight();
            const float carryWeight = GetCarryWeight();
            const float ratio = carryWeight > 0.0f ? totalWeight / carryWeight : 0.0f;

            ImGui::TextDisabled(
                "%s: %zu  |  %s: %.1f  |  %s: %.1f / %.1f  |  %s: %lld",
                L(context, "Inventory", "sTotalItems", "Total Items"),
                cachedInventory.size(),
                L(context, "Inventory", "sTotalWeight", "Total Weight"),
                totalWeight,
                L(context, "Inventory", "sCarryWeight", "Carry Weight"),
                totalWeight,
                carryWeight,
                L(context, "Inventory", "sCaps", "Caps"),
                GetCapsCount());

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

        void DrawInventoryDetails(const InventoryEntry& entry, InventoryTabContext& context)
        {
            FormEntry formEntry{};
            formEntry.formID = entry.formID;
            formEntry.name = entry.name;
            formEntry.category = ResolveCategoryLabel(entry.category, context);
            formEntry.sourcePlugin = entry.sourcePlugin;

            FormDetailsViewContext detailsContext{
                .localize = context.localize,
                .showAdvancedDetailsView = Config::Get().pluginAdvancedDetailsView
            };
            FormDetailsView::Draw(formEntry, detailsContext);

            const std::uint64_t key = MakeRowKey(entry.formID, entry.stackID);
            const int currentFrame = ImGui::GetFrameCount();
            if (key != detailCacheKey || currentFrame - detailCacheFrame >= 30) {
                detailCacheKey = key;
                detailCacheFrame = currentFrame;
                cachedDetailState = BuildInventoryDetailState(entry);
            }
            const auto& state = cachedDetailState;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("%s: %u", L(context, "Inventory", "sQuantity", "Qty"), entry.count);
            ImGui::Text("%s: %s", L(context, "Inventory", "sEquipped", "Equipped"), entry.isEquipped ? L(context, "General", "sYes", "Yes") : L(context, "General", "sNo", "No"));
            ImGui::Text("%s: %s", L(context, "General", "sFavorite", "Favorite"), entry.isFavorited ? L(context, "General", "sYes", "Yes") : L(context, "General", "sNo", "No"));
            if (entry.isQuestItem) {
                ImVec4 questColor = GetCategoryColor("", true, false);
                ImGui::PushStyleColor(ImGuiCol_Text, questColor);
                ImGui::Text("%s: %s", L(context, "Inventory", "sQuestItem", "Quest Item"), L(context, "General", "sYes", "Yes"));
                ImGui::PopStyleColor();
            }
            ImGui::Text("%s: %.2f", L(context, "Inventory", "sWeight", "Weight"), entry.weight);
            ImGui::Text("%s: %d", L(context, "Inventory", "sValue", "Value"), entry.value);
            if (IsWeaponCategory(entry.category)) {
                ImGui::Text("%s: %u", L(context, "Inventory", "sDamageOrRating", "DMG/DR"), entry.damage);
            } else if (IsArmorCategory(entry.category)) {
                ImGui::Text("%s: %u", L(context, "Inventory", "sDamageOrRating", "DMG/DR"), entry.armorRating);
            }

            if (!state.legendaryName.empty()) {
                ImGui::Text("%s: %s", L(context, "Inventory", "sLegendary", "Legendary"), state.legendaryName.c_str());
            }
            if (state.healthPercent >= 0.0f) {
                ImGui::Text("%s: %.0f%%", L(context, "Inventory", "sCondition", "Condition"), state.healthPercent * 100.0f);
            }

            ImGui::Spacing();
            ImGui::TextUnformatted(L(context, "Inventory", "sCurrentMods", "Current Mods"));
            if (state.currentMods.empty()) {
                ImGui::TextDisabled("%s", L(context, "Inventory", "sNoMods", "No mods attached"));
            } else {
                for (const auto& currentMod : state.currentMods) {
                    ImGui::BulletText("%s: %s", currentMod.slotLabel.c_str(), currentMod.name.c_str());
                }
            }

            ImGui::Spacing();
            ImGui::TextUnformatted(L(context, "Inventory", "sEnchantments", "Enchantments"));
            if (state.enchantments.empty()) {
                ImGui::TextDisabled("%s", L(context, "General", "sNone", "None"));
            } else {
                for (const auto& enchantment : state.enchantments) {
                    ImGui::BulletText("%s", enchantment.c_str());
                }
            }
        }

        void DrawCategoryTabs(InventoryTabContext& context)
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
                    activeCategory = category;
                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }

        std::vector<InventoryEntry> CopySelectedEntries(const std::vector<const InventoryEntry*>& visibleEntries)
        {
            std::vector<InventoryEntry> selectedEntries{};
            selectedEntries.reserve(selectedInventoryRows.size());
            for (const auto* entry : visibleEntries) {
                if (selectedInventoryRows.contains(MakeRowKey(entry->formID, entry->stackID))) {
                    selectedEntries.push_back(*entry);
                }
            }
            return selectedEntries;
        }

        void DrawInventoryContextMenu(const InventoryEntry& entry, InventoryTabContext& context)
        {
            const bool gameplayActionsAllowed = FormActions::AreGameplayActionsAllowed();
            const std::string displayName = entry.name.empty() ? std::string(L(context, "General", "sUnnamed", "<Unnamed>")) : entry.name;

            ImGui::TextUnformatted(displayName.c_str());
            const std::string formIDText = FormatUtils::FormID(entry.formID);
            ImGui::TextDisabled("%s  |  %s", formIDText.c_str(), entry.sourcePlugin.c_str());
            ImGui::Separator();

            if (!gameplayActionsAllowed) {
                ImGui::BeginDisabled(true);
            }

            if (ImGui::MenuItem(L(context, "Inventory", "sRemoveItem", "Remove Item"))) {
                char message[512]{};
                std::snprintf(message, sizeof(message), L(context, "Inventory", "sRemoveItemConfirm", "Remove %s (x%d) from inventory?"), displayName.c_str(), entry.count);
                MainWindowPopups::RequestActionConfirmation(
                    L(context, "Inventory", "sRemoveItem", "Remove Item"),
                    message,
                    [entry]() {
                        RemoveInventoryEntry(entry, static_cast<std::int32_t>(entry.count), false);
                    });
            }

            if (ImGui::MenuItem(L(context, "Inventory", "sDropItem", "Drop Item"))) {
                char message[512]{};
                std::snprintf(message, sizeof(message), L(context, "Inventory", "sDropItemConfirm", "Drop %s (x%d)?"), displayName.c_str(), entry.count);
                MainWindowPopups::RequestActionConfirmation(
                    L(context, "Inventory", "sDropItem", "Drop Item"),
                    message,
                    [entry]() {
                        RemoveInventoryEntry(entry, static_cast<std::int32_t>(entry.count), true);
                    });
            }

            static int desiredCount = 1;
            desiredCount = (std::max)(desiredCount, 0);
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt("##SetCountInput", &desiredCount, 1, 10);
            desiredCount = (std::max)(desiredCount, 0);
            if (ImGui::MenuItem(L(context, "Inventory", "sApplySetCount", "Apply Count"))) {
                AdjustInventoryEntryCount(entry, desiredCount);
            }

            if (IsEquippable(entry)) {
                if (ImGui::MenuItem(entry.isEquipped ? L(context, "Inventory", "sUnequipItem", "Unequip") : L(context, "Inventory", "sEquipItem", "Equip"))) {
                    EquipInventoryEntry(entry, !entry.isEquipped);
                }
                if (ImGui::MenuItem(L(context, "Inventory", "sDuplicate", "Duplicate"))) {
                    DuplicateInventoryEntry(entry);
                }
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
                FormActions::CopyFormID(entry.formID);
            }
            if (ImGui::MenuItem(L(context, "General", "sCopyName", "Copy Name"))) {
                ImGui::SetClipboardText(displayName.c_str());
            }
            if (const char* editorID = ContextMenu::TryGetEditorID(entry.formID)) {
                if (ImGui::MenuItem(L(context, "General", "sCopyEditorID", "Copy EditorID"))) {
                    ImGui::SetClipboardText(editorID);
                }
            }
            if (ImGui::MenuItem(L(context, "General", "sCopyRecordSource", "Copy Record Source"))) {
                ImGui::SetClipboardText(entry.sourcePlugin.c_str());
            }
            if (context.inspectFormInPluginBrowser && ImGui::MenuItem(L(context, "Inventory", "sInspectInBrowser", "Inspect in Plugin Browser"))) {
                context.inspectFormInPluginBrowser(entry.formID);
            }
        }
    }

    void InventoryTab::Draw(InventoryTabContext& context)
    {
        RefreshPlayerInventory();
        context.playerGodModeEnabled = FormActions::IsPlayerGodModeEnabled();

        DrawQuickActions(context);
        SearchBar::Draw(L(context, "Inventory", "sSearch", "Search Inventory..."), inventorySearchBuffer, sizeof(inventorySearchBuffer), inventorySearch, context.searchFocusPending);
        ImGui::SameLine();
        ImGui::Checkbox(L(context, "Inventory", "sEquipped", "Equipped"), &showEquippedOnly);
        ImGui::SameLine();
        if (ImGui::Button(L(context, "Inventory", "sRefreshInventory", "Refresh"))) {
            MarkRefreshNeeded();
            RefreshPlayerInventory();
        }

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
                MainWindowPopups::RequestActionConfirmation(
                    L(context, "Inventory", "sRemoveSelected", "Remove Selected"),
                    L(context, "General", "sConfirmAction", "Confirm Action"),
                    [entries = selectedEntries]() {
                        for (const auto& entry : entries) {
                            RemoveInventoryEntry(entry, static_cast<std::int32_t>(entry.count), false);
                        }
                    });
            }
            if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sDropSelected", "Drop Selected"), firstButton)) {
                MainWindowPopups::RequestActionConfirmation(
                    L(context, "Inventory", "sDropSelected", "Drop Selected"),
                    L(context, "General", "sConfirmAction", "Confirm Action"),
                    [entries = selectedEntries]() {
                        for (const auto& entry : entries) {
                            RemoveInventoryEntry(entry, static_cast<std::int32_t>(entry.count), true);
                        }
                    });
            }
            if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sEquipSelected", "Equip Selected"), firstButton)) {
                for (const auto& entry : selectedEntries) {
                    if (IsEquippable(entry)) {
                        EquipInventoryEntry(entry, true);
                    }
                }
            }
            if (ImGuiWidgetUtils::DrawWrappedButton(L(context, "Inventory", "sUnequipSelected", "Unequip Selected"), firstButton)) {
                for (const auto& entry : selectedEntries) {
                    if (IsEquippable(entry)) {
                        EquipInventoryEntry(entry, false);
                    }
                }
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

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(visibleEntries.size()));
                while (clipper.Step()) {
                    for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                        const auto& entry = *visibleEntries[static_cast<std::size_t>(rowIndex)];
                        const std::uint64_t rowKey = MakeRowKey(entry.formID, entry.stackID);
                        const bool rowSelected = selectedInventoryRows.contains(rowKey);

                        ImGui::PushID(static_cast<int>(entry.formID));
                        ImGui::PushID(static_cast<int>(entry.stackID));
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
                        const bool clicked = ImGui::Selectable(rowLabel.c_str(), rowSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
                        if (clicked) {
                            if (ImGui::GetIO().KeyShift && lastClickedInventoryRow >= 0 && lastClickedInventoryRow < static_cast<int>(visibleEntries.size())) {
                                const int rangeStart = (std::min)(lastClickedInventoryRow, rowIndex);
                                const int rangeEnd = (std::max)(lastClickedInventoryRow, rowIndex);
                                if (!ImGui::GetIO().KeyCtrl) {
                                    selectedInventoryRows.clear();
                                }
                                for (int index = rangeStart; index <= rangeEnd; ++index) {
                                    const auto* rangeEntry = visibleEntries[static_cast<std::size_t>(index)];
                                    selectedInventoryRows.insert(MakeRowKey(rangeEntry->formID, rangeEntry->stackID));
                                }
                            } else if (ImGui::GetIO().KeyCtrl) {
                                if (rowSelected) {
                                    selectedInventoryRows.erase(rowKey);
                                } else {
                                    selectedInventoryRows.insert(rowKey);
                                }
                            } else {
                                selectedInventoryRows.clear();
                                selectedInventoryRows.insert(rowKey);
                            }

                            lastClickedInventoryRow = rowIndex;

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
                            selectedInventoryRows.clear();
                            selectedInventoryRows.insert(rowKey);
                            lastClickedInventoryRow = rowIndex;
                        }
                        ImGui::OpenPopupOnItemClick("InventoryRowContext", ImGuiPopupFlags_MouseButtonRight);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(ResolveCategoryLabel(entry.category, context).c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%u", entry.count);
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
                            DrawInventoryContextMenu(entry, context);
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
        if (lastClickedInventoryRow >= 0 && lastClickedInventoryRow < static_cast<int>(visibleEntries.size())) {
            const auto* candidate = visibleEntries[static_cast<std::size_t>(lastClickedInventoryRow)];
            if (selectedInventoryRows.contains(MakeRowKey(candidate->formID, candidate->stackID))) {
                selectedEntry = candidate;
            }
        }
        if (!selectedEntry) {
            for (const auto* entry : visibleEntries) {
                if (selectedInventoryRows.contains(MakeRowKey(entry->formID, entry->stackID))) {
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
                MainWindowPopups::RequestActionConfirmation(
                    L(context, "Inventory", "sRemoveSelected", "Remove Selected"),
                    L(context, "General", "sConfirmAction", "Confirm Action"),
                    [entries = selectedEntries]() {
                        for (const auto& entry : entries) {
                            RemoveInventoryEntry(entry, static_cast<std::int32_t>(entry.count), false);
                        }
                    });
            }

            if (selectedEntry && ImGui::IsKeyPressed(ImGuiKey_E, false) && IsEquippable(*selectedEntry)) {
                EquipInventoryEntry(*selectedEntry, !selectedEntry->isEquipped);
            }
        }
    }

    void InventoryTab::ResetState()
    {
        cachedInventory.clear();
        lastRefreshFrame = -30;
        forceRefresh = true;
        inventorySearch.clear();
        inventorySearchBuffer[0] = '\0';
        activeCategory = InventoryCategoryTab::All;
        showEquippedOnly = false;
        selectedInventoryRows.clear();
        lastClickedInventoryRow = -1;
        detailCacheKey = 0;
        detailCacheFrame = -30;
        cachedDetailState = {};
    }
}
