#pragma once

#include "Core/InventorySnapshot.h"

#include <cmath>
#include <optional>
#include <span>
#include <unordered_set>

namespace ESPExplorerAE
{
    enum class InventoryAction { Remove, Drop, Equip, Unequip, Use, DuplicateItem };
    enum class InventoryRejection { None, Unavailable, StaleSession, Invalid, MissingStack, ChangedStack, QuestItem, AmbiguousInstance, TooLarge };

    struct InventoryStep
    {
        InventoryIndex stack{};
        std::uint32_t count{};
    };

    struct InventoryPlan
    {
        std::shared_ptr<const InventorySnapshot> source;
        InventoryAction action{};
        std::vector<InventoryStep> steps;
    };

    struct InventoryPreparation
    {
        InventoryRejection rejection{ InventoryRejection::Invalid };
        InventoryPlan plan;
        explicit operator bool() const { return rejection == InventoryRejection::None; }
    };

    inline InventoryRejection ValidateInventoryBaseAddition(const std::shared_ptr<const InventorySnapshot>& source,
        std::span<const InventoryIndex> indices, std::uint32_t count)
    {
        if (!source || !source->ready || !source->session || !source->generation) return InventoryRejection::Unavailable;
        if (!count || indices.empty()) return InventoryRejection::Invalid;
        if (count > 1000000 || indices.size() > 256) return InventoryRejection::TooLarge;
        std::unordered_set<InventoryIndex> unique;
        const InventoryStack* group{};
        for (const auto index : indices) {
            if (index >= source->stacks.size() || !unique.insert(index).second) return InventoryRejection::Invalid;
            const auto& stack = source->stacks[index];
            if (!stack.formID || !stack.token || !stack.count || stack.isDeleted || !stack.instanceIdentityKnown ||
                !std::isfinite(stack.weight) || !std::isfinite(stack.healthPercent)) return InventoryRejection::Invalid;
            if (group && (stack.formID != group->formID || stack.name != group->name)) return InventoryRejection::Invalid;
            group = &stack;
        }
        // The entire source group is required. A final-count calculation cannot
        // be based on a silently shortened selection of same-name stacks.
        for (InventoryIndex index = 0; index < source->stacks.size(); ++index) {
            const auto& stack = source->stacks[index];
            if (stack.formID == group->formID && stack.name == group->name && !unique.contains(index)) return InventoryRejection::Invalid;
        }
        return InventoryRejection::None;
    }

    inline InventoryRejection RevalidateInventoryBaseAddition(const std::shared_ptr<const InventorySnapshot>& source,
        std::span<const InventoryIndex> indices, std::uint32_t count, const InventorySnapshot& current)
    {
        const auto valid = ValidateInventoryBaseAddition(source, indices, count);
        if (valid != InventoryRejection::None) return valid;
        if (source->session != current.session) return InventoryRejection::StaleSession;
        if (!current.ready || !current.generation) return InventoryRejection::Unavailable;
        if (current.generation < source->generation) return InventoryRejection::ChangedStack;
        const auto& group = source->stacks[indices.front()];
        std::size_t members{};
        for (const auto& stack : current.stacks) if (stack.formID == group.formID && stack.name == group.name) ++members;
        if (members != indices.size()) return InventoryRejection::ChangedStack;
        for (const auto index : indices) {
            const auto& expected = source->stacks[index];
            const auto* actual = current.Find(expected.token);
            if (!actual) return InventoryRejection::MissingStack;
            if (*actual != expected) return InventoryRejection::ChangedStack;
        }
        return InventoryRejection::None;
    }

    inline bool InventoryActionAllowed(const InventoryStack& stack, InventoryAction action)
    {
        if (stack.isDeleted || !stack.instanceIdentityKnown || !std::isfinite(stack.weight) || !std::isfinite(stack.healthPercent)) return false;
        switch (action) {
        case InventoryAction::Remove: case InventoryAction::Drop: return !stack.isQuestItem;
        case InventoryAction::Equip: case InventoryAction::Unequip: return stack.category == "WEAP" || stack.category == "ARMO";
        case InventoryAction::Use: return stack.category == "ALCH" || stack.category == "BOOK" || stack.category == "NOTE";
        case InventoryAction::DuplicateItem: return stack.formID && stack.token && stack.count;
        default: return false;
        }
    }

    // A destructive group action consumes its explicitly captured members in
    // token order. Instance actions require a single selected stack; the caller
    // must not silently choose among same-name items with different instances.
    inline InventoryPreparation PrepareInventoryAction(std::shared_ptr<const InventorySnapshot> source,
        std::span<const InventoryIndex> indices, InventoryAction action, std::uint64_t count)
    {
        InventoryPreparation result;
        if (!source || !source->ready || !source->session || !source->generation) { result.rejection = InventoryRejection::Unavailable; return result; }
        if (indices.empty() || !count) return result;
        const bool destructive = action == InventoryAction::Remove || action == InventoryAction::Drop;
        if (!destructive && (indices.size() != 1 || count != 1)) { result.rejection = InventoryRejection::AmbiguousInstance; return result; }
        if (indices.size() > 256 || count > 1000000) { result.rejection = InventoryRejection::TooLarge; return result; }
        std::unordered_set<InventoryIndex> unique;
        std::uint64_t available{};
        for (const auto index : indices) {
            if (index >= source->stacks.size() || !unique.insert(index).second) return result;
            const auto& stack = source->stacks[index];
            if (destructive && stack.isQuestItem) { result.rejection = InventoryRejection::QuestItem; return result; }
            if (!InventoryActionAllowed(stack, action)) return result;
            available += stack.count;
        }
        if (count > available) return result;
        result.plan = { std::move(source), action, {} };
        auto ordered = std::vector<InventoryIndex>(indices.begin(), indices.end());
        std::ranges::sort(ordered, {}, [&](InventoryIndex index) { return result.plan.source->stacks[index].token; });
        for (const auto index : ordered) {
            const auto quantity = static_cast<std::uint32_t>((std::min)(count, static_cast<std::uint64_t>(result.plan.source->stacks[index].count)));
            if (quantity) result.plan.steps.push_back({ index, quantity });
            count -= quantity;
            if (!count) break;
        }
        result.rejection = InventoryRejection::None;
        return result;
    }

    // Called again immediately before each engine operation, after resolving
    // its token against the current inventory. Ordinals never cross this seam.
    inline InventoryRejection RevalidateInventoryStep(const InventoryPlan& plan, const InventoryStep& step, const InventorySnapshot& current)
    {
        if (!plan.source || !plan.source->ready || !plan.source->session || !plan.source->generation || step.stack >= plan.source->stacks.size() || !step.count || step.count > 1000000) return InventoryRejection::Invalid;
        if (plan.source->session != current.session) return InventoryRejection::StaleSession;
        if (!current.ready || !current.generation) return InventoryRejection::Unavailable;
        if (current.generation < plan.source->generation) return InventoryRejection::ChangedStack;
        const auto& expected = plan.source->stacks[step.stack];
        const auto* actual = current.Find(expected.token);
        if (!actual) return InventoryRejection::MissingStack;
        if (expected != *actual) return InventoryRejection::ChangedStack;
        if ((plan.action == InventoryAction::Remove || plan.action == InventoryAction::Drop) && actual->isQuestItem) return InventoryRejection::QuestItem;
        if (!InventoryActionAllowed(*actual, plan.action) || step.count > actual->count ||
            ((plan.action == InventoryAction::Equip || plan.action == InventoryAction::Unequip || plan.action == InventoryAction::Use ||
                plan.action == InventoryAction::DuplicateItem) && step.count != 1)) return InventoryRejection::Invalid;
        return InventoryRejection::None;
    }
}
