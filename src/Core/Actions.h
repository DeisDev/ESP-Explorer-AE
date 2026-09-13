#pragma once

#include "Core/InventoryActions.h"

#include <cstddef>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ESPExplorerAE
{
    enum class ActionKind
    {
        Give, GiveWithAmmo, Spawn, Place, AddSpell, RemoveSpell, AddPerk,
        RemovePerk, Outfit, ConstructedItem, CurrentAmmo, GodMode, Console, Teleport, Equip,
        StartQuest, CompleteQuest, SetWeather, PlaySound, SetGlobal,
        InventoryRemove, InventoryDrop, InventoryEquip, InventoryUnequip, InventoryUse, InventoryAddBase, InventoryDuplicateItem,
        RestoreHealth, ToggleNoClip, SetPlayerLevel, AddPerkPoints, SetGameHour
    };

    enum class ActionStatus { Rejected, Dispatched, VerifiedChanged, NoChange, Failed };
    enum class ActionAdmission { Accepted, Unavailable, StaleSession, Invalid, QueueFull, BatchTooLarge };

    struct ActionRequest
    {
        ActionKind kind{ ActionKind::Give };
        std::uint32_t formID{};
        std::uint32_t count{ 1 };
        std::uint32_t ammoFormID{};
        std::uint32_t ammoCount{};
        std::string command;
        std::uint64_t session{};
        bool substituteComponents{ true };
        bool debugOverride{};
        float value{};
        std::shared_ptr<const InventorySnapshot> inventory;
        InventoryIndex inventoryStack{};
        std::vector<InventoryIndex> inventoryGroup;
        std::uint64_t groupID{};
        std::string groupName;
    };

    inline std::optional<InventoryAction> InventoryOperation(ActionKind kind)
    {
        switch (kind) {
        case ActionKind::InventoryRemove: return InventoryAction::Remove;
        case ActionKind::InventoryDrop: return InventoryAction::Drop;
        case ActionKind::InventoryEquip: return InventoryAction::Equip;
        case ActionKind::InventoryUnequip: return InventoryAction::Unequip;
        case ActionKind::InventoryUse: return InventoryAction::Use;
        case ActionKind::InventoryDuplicateItem: return InventoryAction::DuplicateItem;
        default: return {};
        }
    }

    inline ActionKind InventoryOperationKind(InventoryAction action)
    {
        switch (action) {
        case InventoryAction::Remove: return ActionKind::InventoryRemove;
        case InventoryAction::Drop: return ActionKind::InventoryDrop;
        case InventoryAction::Equip: return ActionKind::InventoryEquip;
        case InventoryAction::Unequip: return ActionKind::InventoryUnequip;
        case InventoryAction::Use: return ActionKind::InventoryUse;
        case InventoryAction::DuplicateItem: return ActionKind::InventoryDuplicateItem;
        default: return static_cast<ActionKind>(-1);
        }
    }

    struct ActionEffect
    {
        std::uint32_t formID{};
        std::uint32_t count{};
        std::string name;
        ActionStatus status{ ActionStatus::Rejected };
        std::optional<std::int64_t> before;
        std::optional<std::int64_t> after;
    };

    struct ActionOutcome
    {
        ActionStatus status{ ActionStatus::Rejected };
        std::string targetName;
        std::vector<ActionEffect> effects;
        InventoryRejection inventoryRejection{ InventoryRejection::None };

        void Finish()
        {
            if (effects.empty()) return;
            bool rejected = false;
            bool dispatched = false;
            bool changed = false;
            for (const auto& effect : effects) {
                if (effect.status == ActionStatus::Failed) { status = ActionStatus::Failed; return; }
                rejected |= effect.status == ActionStatus::Rejected;
                dispatched |= effect.status == ActionStatus::Dispatched;
                changed |= effect.status == ActionStatus::VerifiedChanged;
            }
            // Preserve individual effects; a partly dispatched batch must never
            // be silently retried or compensated twice.
            status = rejected ? ((dispatched || changed) ? ActionStatus::Failed : ActionStatus::Rejected) :
                dispatched ? ActionStatus::Dispatched : changed ? ActionStatus::VerifiedChanged : ActionStatus::NoChange;
        }
    };

    struct ActionReceipt
    {
        std::uint64_t id{};
        std::uint64_t session{};
        ActionRequest request;
        ActionOutcome outcome;
    };

    class ActionQueue
    {
    public:
        static constexpr std::size_t Capacity = 256;
        static constexpr std::uint32_t MaxQuantity = 1000000;

        void BeginSession(bool available)
        {
            ++session;
            ready = available;
            pending.clear();
        }
        void SetReady(bool available) { ready = available; }
        bool IsReady() const { return ready; }
        std::uint64_t Session() const { return session; }
        std::size_t Size() const { return pending.size(); }

        bool Submit(ActionRequest request)
        {
            if (!ready || (request.session && request.session != session) || pending.size() >= Capacity || !Valid(request)) return false;
            request.session = session;
            pending.push_back(std::move(request));
            return true;
        }

        ActionAdmission SubmitBatch(std::span<const ActionRequest> requests)
        {
            if (requests.empty()) return ActionAdmission::Accepted;
            if (!ready) return ActionAdmission::Unavailable;
            if (requests.size() > Capacity) return ActionAdmission::BatchTooLarge;
            if (requests.size() > Capacity - pending.size()) return ActionAdmission::QueueFull;
            for (const auto& request : requests) {
                if (request.session && request.session != session) return ActionAdmission::StaleSession;
                if (!Valid(request)) return ActionAdmission::Invalid;
            }
            // Stage the bounded queue before commit, including allocations and
            // owned command strings. Rejection or a copy exception admits none.
            auto next = pending;
            for (auto request : requests) {
                request.session = session;
                next.push_back(std::move(request));
            }
            pending.swap(next);
            return ActionAdmission::Accepted;
        }

        std::optional<ActionRequest> Pop()
        {
            if (!ready || pending.empty()) return std::nullopt;
            auto request = std::move(pending.front());
            pending.pop_front();
            if (request.session != session) return std::nullopt;
            return request;
        }

        static bool Valid(const ActionRequest& request)
        {
            if (request.groupName.size() > 128) return false;
            if (request.kind < ActionKind::Give || request.kind > ActionKind::SetGameHour) return false;
            if (request.count == 0 || request.count > MaxQuantity || request.ammoCount > MaxQuantity) return false;
            if (request.kind == ActionKind::SetGlobal && !std::isfinite(request.value)) return false;
            if (request.kind == ActionKind::InventoryAddBase) {
                if (ValidateInventoryBaseAddition(request.inventory, request.inventoryGroup, request.count) != InventoryRejection::None) return false;
                return request.session == request.inventory->session && request.formID == request.inventory->stacks[request.inventoryGroup.front()].formID;
            }
            if (!request.inventoryGroup.empty()) return false;
            if (const auto operation = InventoryOperation(request.kind)) {
                if (!request.inventory || request.session != request.inventory->session || request.inventoryStack >= request.inventory->stacks.size()) return false;
                const auto& stack = request.inventory->stacks[request.inventoryStack];
                const InventoryPlan plan{ request.inventory, *operation, {} };
                return request.formID == stack.formID && RevalidateInventoryStep(plan, { request.inventoryStack, request.count }, *request.inventory) == InventoryRejection::None;
            }
            if (request.inventory) return false;
            if (request.kind == ActionKind::Console) return !request.command.empty() && request.command.size() <= 4096;
            if (request.kind == ActionKind::GodMode) return request.ammoCount <= 1;
            if (request.kind == ActionKind::CurrentAmmo || request.kind == ActionKind::RestoreHealth || request.kind == ActionKind::ToggleNoClip) return true;
            if (request.kind == ActionKind::SetPlayerLevel) return request.count <= 65535;
            if (request.kind == ActionKind::AddPerkPoints) return request.count <= 999;
            if (request.kind == ActionKind::SetGameHour) return std::isfinite(request.value) && request.value >= 0.0f && request.value < 24.0f;
            if (request.formID == 0) return false;
            if (request.kind == ActionKind::GiveWithAmmo && request.ammoCount > 0 && request.ammoFormID == 0) return false;
            return true;
        }

    private:
        std::uint64_t session{};
        bool ready{};
        std::deque<ActionRequest> pending;
    };

    inline std::vector<ActionRequest> InventoryRequests(const InventoryPreparation& prepared)
    {
        std::vector<ActionRequest> requests;
        if (!prepared || !prepared.plan.source) return requests;
        for (const auto& step : prepared.plan.steps) {
            const auto& stack = prepared.plan.source->stacks.at(step.stack);
            requests.push_back({ .kind = InventoryOperationKind(prepared.plan.action), .formID = stack.formID,
                .count = step.count, .session = prepared.plan.source->session, .inventory = prepared.plan.source, .inventoryStack = step.stack });
        }
        return requests;
    }
}
