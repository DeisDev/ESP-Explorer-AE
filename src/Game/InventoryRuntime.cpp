#include "Game/InventoryRuntime.h"
#include "pch.h"
#include "Game/ActionExecutor.h"
#include "Game/WeaponInstanceCopy.h"
#include "Core/StandardForms.h"
#include <RE/A/ActorEquipManager.h>

#include <RE/A/ActorValue.h>
#include <RE/B/BGSAttachParentArray.h>
#include <RE/B/BGSObjectInstanceExtra.h>
#include <RE/E/ExtraUniqueID.h>
#include <RE/E/ExtraInstanceData.h>
#include <RE/T/TESValueForm.h>
#include <RE/T/TESWeightForm.h>

namespace ESPExplorerAE
{
    namespace
    {
        struct RetainedStack
        {
            std::uint64_t token{};
            std::uint64_t instanceRevision{ 1 };
            std::uint32_t formID{};
            RE::BSTSmartPointer<RE::BGSInventoryItem::Stack> stack;
            RE::BSTSmartPointer<RE::ExtraDataList> extra;
            RE::TBO_InstanceData* instance{};
            RE::BSTSmartPointer<RE::TBO_InstanceData> ownedInstance;
        };
        struct RuntimeState
        {
            std::unordered_map<const RE::BGSInventoryItem::Stack*, RetainedStack> retained;
            std::shared_ptr<const InventorySnapshot> published;
            std::uint64_t activeSession{};
            std::uint64_t nextToken{ 1 };
            std::uint64_t nextGeneration{ 1 };
        };

        RuntimeState& Runtime()
        {
            // Engine handles are released explicitly by Reset on the game task.
            // A CRT/DLL-detach destructor must not delete engine objects while
            // the loader lock is held or the game's allocator is shutting down.
            static auto* state = new RuntimeState;
            return *state;
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

            return std::format("{:08X}", form->GetFormID());
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

            return std::format("{:08X}", keyword->GetFormID());
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

        InventoryStack ReadStack(RE::BGSInventoryItem& item, RE::BGSInventoryItem::Stack& stack,
            std::uint32_t ordinal, const RetainedStack& identity)
        {
            InventoryStack result;
            result.token = identity.token;
            result.instanceRevision = identity.instanceRevision;
            result.formID = identity.formID;
            result.count = stack.GetCount();
            result.flags = stack.flags.underlying();
            result.isEquipped = stack.IsEquipped();
            result.category = ResolveCategoryCode(item.object);
            result.sourcePlugin = ResolvePluginName(item.object);
            result.name = ResolveName(item, stack.extra.get(), item.object);
            result.isQuestItem = item.IsQuestObject(ordinal);
            result.isDeleted = item.object->IsDeleted();
            auto* instance = identity.instance;
            result.instanceIdentityKnown = !instance || identity.ownedInstance.get() == instance || item.object->GetBaseInstanceData() == instance;
            result.weight = RE::TESWeightForm::GetFormWeight(item.object, instance);
            result.value = static_cast<std::int32_t>(RE::TESValueForm::GetFormValue(item.object, instance));
            // Engine objects use the game's RTTI descriptors, not the DLL's
            // compiler-generated type information for the RE declarations.
            if (const auto* data = instance ? RE::fallout_cast<const RE::TESObjectWEAP::InstanceData*>(instance) : nullptr) result.damage = data->attackDamage;
            else if (const auto* weapon = item.object->As<RE::TESObjectWEAP>()) result.damage = weapon->weaponData.attackDamage;
            if (const auto* data = instance ? RE::fallout_cast<const RE::TESObjectARMO::InstanceData*>(instance) : nullptr) result.armorRating = data->rating;
            else if (const auto* armor = item.object->As<RE::TESObjectARMO>()) result.armorRating = armor->armorData.rating;
            if (auto* extra = identity.extra.get()) {
                result.isFavorited = extra->IsFavorite();
                result.healthPercent = extra->GetHealthPerc();
                if (const auto* unique = extra->GetByType<RE::ExtraUniqueID>()) {
                    result.uniqueBaseID = unique->baseID;
                    result.uniqueID = unique->uniqueID;
                }
                if (auto* legendary = extra->GetLegendaryMod()) {
                    result.isLegendary = true;
                    result.legendaryFormID = legendary->GetFormID();
                    result.legendaryName = ResolveFormLabel(legendary);
                }
                // BGSObjectInstanceExtra may have no values buffer. The pinned
                // GetIndexData implementation dereferences it unconditionally.
                if (auto* objectInstance = extra->GetByType<RE::BGSObjectInstanceExtra>(); objectInstance && objectInstance->values) {
                    result.modCount = objectInstance->GetNumMods(false);
                    for (const auto& modData : objectInstance->GetIndexData()) {
                        InventoryMod mod;
                        mod.formID = modData.objectID;
                        mod.attachIndex = modData.index;
                        mod.rank = modData.rank;
                        mod.disabled = modData.disabled;
                        mod.slotLabel = ResolveSlotLabel(item.object, modData.index);
                        mod.name = ResolveFormLabel(RE::TESForm::GetFormByID(mod.formID));
                        result.mods.push_back(std::move(mod));
                    }
                }
            }
            if (const auto* enchantments = instance ? instance->GetEnchantmentArray() : nullptr) {
                for (const auto* enchantment : *enchantments) {
                    if (enchantment) result.enchantments.push_back({ enchantment->GetFormID(), ResolveFormLabel(enchantment) });
                }
            }
            return result;
        }
    }

    void InventoryRuntime::Reset()
    {
        // Called on the game task, never from a render/session callback.
        auto& state = Runtime();
        state.retained.clear();
        state.published.reset();
        state.activeSession = 0;
    }

    std::shared_ptr<const InventorySnapshot> InventoryRuntime::Capture(std::uint64_t session)
    {
        auto& state = Runtime();
        auto& retained = state.retained;
        auto& published = state.published;
        auto& activeSession = state.activeSession;
        if (activeSession != session) { Reset(); activeSession = session; }
        auto next = std::make_shared<InventorySnapshot>();
        next->session = session;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!session || !player || !player->inventoryList) return next;
        std::unordered_map<const RE::BGSInventoryItem::Stack*, RetainedStack> nextRetained;
        {
            RE::BSAutoReadLock lock{ player->inventoryList->rwLock };
            for (auto& item : player->inventoryList->data) {
                if (!item.object || !item.object->GetFormID()) continue;
                std::uint32_t ordinal{};
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++ordinal) {
                    if (!stack->GetCount()) continue;
                    RetainedStack identity;
                    const auto previous = retained.find(stack);
                    auto* instance = item.GetInstanceData(ordinal);
                    if (previous != retained.end() && previous->second.formID == item.object->GetFormID()) {
                        identity.token = previous->second.token;
                        identity.instanceRevision = previous->second.instanceRevision;
                        if (previous->second.extra.get() != stack->extra.get() || previous->second.instance != instance) ++identity.instanceRevision;
                    } else identity.token = state.nextToken++;
                    identity.formID = item.object->GetFormID();
                    identity.stack.reset(stack);
                    identity.extra = stack->extra;
                    identity.instance = instance;
                    // Copy an established owning handle. Base instance data can
                    // be embedded in a form and must never be adopted/deleted.
                    if (const auto* data = stack->extra ? stack->extra->GetByType<RE::ExtraInstanceData>() : nullptr) identity.ownedInstance = data->data;
                    next->stacks.push_back(ReadStack(item, *stack, ordinal, identity));
                    if (!nextRetained.emplace(stack, std::move(identity)).second) throw std::runtime_error("inventory stack belongs to multiple items");
                }
            }
        }
        // Old references survive enumeration, preventing address reuse from
        // assigning an old token to a different object. Removed tokens retire.
        if (const auto* values = RE::ActorValue::GetSingleton(); values && values->carryWeight) next->carryWeight = player->GetActorValue(*values->carryWeight);
        for (const auto& stack : next->stacks) if (stack.formID == StandardForms::Caps) next->capsCount += stack.count;
        std::ranges::sort(next->stacks, {}, &InventoryStack::token);
        const bool unchanged = published && published->session == session && published->stacks == next->stacks;
        if (unchanged && published->carryWeight == next->carryWeight && published->capsCount == next->capsCount) {
            retained = std::move(nextRetained);
            return published;
        }
        next->generation = unchanged ? published->generation : state.nextGeneration++;
        next->ready = true;
        next->BuildGroups();
        retained = std::move(nextRetained);
        published = next;
        return published;
    }

    ActionOutcome InventoryRuntime::Execute(const ActionRequest& request, const std::function<bool()>& isCurrent)
    {
        ActionOutcome outcome;
        const auto operation = InventoryOperation(request.kind);
        const bool addBase = request.kind == ActionKind::InventoryAddBase;
        if ((!operation && !addBase) || !ActionQueue::Valid(request)) { outcome.inventoryRejection = InventoryRejection::Invalid; return outcome; }
        const auto ready = [&] { return isCurrent() && ActionExecutor::AreGameplayActionsAllowed(request.debugOverride); };
        if (!ready()) { outcome.inventoryRejection = isCurrent() ? InventoryRejection::Unavailable : InventoryRejection::StaleSession; return outcome; }
        auto& runtime = Runtime();
        if (runtime.activeSession != request.session) { outcome.inventoryRejection = InventoryRejection::StaleSession; return outcome; }
        if (addBase) {
            const auto current = Capture(request.session);
            outcome.inventoryRejection = RevalidateInventoryBaseAddition(request.inventory, request.inventoryGroup, request.count, *current);
            if (outcome.inventoryRejection != InventoryRejection::None || !ready()) return outcome;
            return ActionExecutor::Execute({ .kind = ActionKind::Give, .formID = request.formID, .count = request.count, .session = request.session,
                .substituteComponents = request.substituteComponents, .debugOverride = request.debugOverride }, isCurrent);
        }
        const auto& expected = request.inventory->stacks[request.inventoryStack];
        outcome.targetName = expected.name;
        auto retainedTarget = std::ranges::find_if(runtime.retained, [&](const auto& entry) { return entry.second.token == expected.token; });
        if (retainedTarget == runtime.retained.end()) { outcome.inventoryRejection = InventoryRejection::MissingStack; return outcome; }
        // Owning copies survive a synchronous lifecycle notification/reset.
        auto identity = retainedTarget->second;
        auto* player = RE::PlayerCharacter::GetSingleton();
        RE::TESBoundObject* object{};
        RE::TBO_InstanceData* instance{};
        std::uint32_t ordinal{};
        InventorySnapshot current;
        current.session = request.session;
        current.generation = request.inventory->generation;
        current.ready = player && player->inventoryList;
        if (player && player->inventoryList) {
            RE::BSAutoReadLock lock{ player->inventoryList->rwLock };
            for (auto& item : player->inventoryList->data) {
                if (!item.object || item.object->GetFormID() != expected.formID) continue;
                std::uint32_t index{};
                for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get(), ++index) {
                    if (stack != identity.stack.get()) continue;
                    instance = item.GetInstanceData(index);
                    if (identity.extra.get() != stack->extra.get() || identity.instance != instance) ++identity.instanceRevision;
                    identity.extra = stack->extra;
                    identity.instance = instance;
                    identity.ownedInstance.reset();
                    if (const auto* data = stack->extra ? stack->extra->GetByType<RE::ExtraInstanceData>() : nullptr) identity.ownedInstance = data->data;
                    current.stacks.push_back(ReadStack(item, *stack, index, identity));
                    current.byToken.emplace(expected.token, 0);
                    object = item.object;
                    ordinal = index;
                    break;
                }
                if (object) break;
            }
        }
        const InventoryPlan plan{ request.inventory, *operation, {} };
        outcome.inventoryRejection = RevalidateInventoryStep(plan, { request.inventoryStack, request.count }, current);
        if (outcome.inventoryRejection != InventoryRejection::None || !object || !ready()) return outcome;
        ActionEffect effect{ .formID = expected.formID, .count = request.count, .name = expected.name };
        const bool destructive = *operation == InventoryAction::Remove || *operation == InventoryAction::Drop;
        const bool equipAction = *operation == InventoryAction::Equip || *operation == InventoryAction::Unequip;
        if (*operation == InventoryAction::DuplicateWeapon) {
            auto* weapon = object->As<RE::TESObjectWEAP>();
            auto copy = weapon ? CopyWeaponInstanceExtra(*weapon, identity.extra.get(), instance) : OwnedInventoryExtra{};
            if (!copy) {
                effect.status = ActionStatus::Failed;
            } else if (ready()) {
                effect.before = player->GetInventoryObjectCount(object);
                // Pass a new owning engine handle while retaining our reference
                // through dispatch/observation. Never hand the original extra
                // list to an insertion API, or change the source stack count.
                player->AddInventoryItem(object, RE::BSTSmartPointer<RE::ExtraDataList>{ copy.get() }, 1, nullptr, nullptr, nullptr);
                effect.status = ActionStatus::Dispatched;
                if (ready()) {
                    try {
                        effect.after = player->GetInventoryObjectCount(object);
                        if (*effect.after == *effect.before + 1) effect.status = ActionStatus::VerifiedChanged;
                    } catch (...) { REX::WARN("Weapon copy dispatched but count observation failed for {:08X}", expected.formID); }
                }
            } else outcome.inventoryRejection = isCurrent() ? InventoryRejection::Unavailable : InventoryRejection::StaleSession;
        } else if (equipAction && expected.isEquipped == (*operation == InventoryAction::Equip)) {
            effect.status = ActionStatus::NoChange;
            effect.before = expected.isEquipped;
            effect.after = effect.before;
        } else {
            if (destructive) effect.before = player->GetInventoryObjectCount(object);
            else if (equipAction) effect.before = expected.isEquipped;
            // Inventory APIs take their own locks. The traversal lock above is
            // released before mutation, and the current ordinal is used once.
            if (destructive) {
                RE::TESObjectREFR::RemoveItemData data{ object, static_cast<std::int32_t>(request.count) };
                data.stackData.push_back(ordinal);
                if (*operation == InventoryAction::Drop) data.reason = RE::ITEM_REMOVE_REASON::KDropping;
                player->RemoveItem(data);
                effect.status = ActionStatus::Dispatched;
            } else if (auto* manager = RE::ActorEquipManager::GetSingleton()) {
                RE::BGSObjectInstance target{ object, instance };
                const bool accepted = *operation == InventoryAction::Unequip ?
                    manager->UnequipObject(player, &target, 1, nullptr, ordinal, false, true, true, true, nullptr) :
                    manager->EquipObject(player, target, ordinal, 1, nullptr, false, true, true, true, false);
                effect.status = accepted ? ActionStatus::Dispatched : ActionStatus::Failed;
            }
            // Loss of observation after dispatch does not turn it into a safe
            // retry or claim that an exact inverse exists.
            if (effect.status == ActionStatus::Dispatched && ready()) {
                try {
                    if (destructive) effect.after = player->GetInventoryObjectCount(object);
                    else if (equipAction && player->inventoryList) {
                        RE::BSAutoReadLock lock{ player->inventoryList->rwLock };
                        for (const auto& item : player->inventoryList->data) {
                            if (!item.object || item.object->GetFormID() != expected.formID) continue;
                            for (auto* stack = item.stackData.get(); stack; stack = stack->nextStack.get()) {
                                if (stack == identity.stack.get()) { effect.after = stack->IsEquipped(); break; }
                            }
                        }
                    }
                    if (effect.before && effect.after && effect.before != effect.after) effect.status = ActionStatus::VerifiedChanged;
                } catch (...) { REX::WARN("Inventory action dispatched but result observation failed for {:08X}", expected.formID); }
            }
        }
        outcome.effects.push_back(std::move(effect));
        outcome.Finish();
        return outcome;
    }
}
