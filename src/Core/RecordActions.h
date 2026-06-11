#pragma once

#include "Core/Actions.h"
#include "Core/FormEntry.h"
#include "Core/CatalogSnapshot.h"
#include <array>
#include <string_view>
#include <unordered_set>

namespace ESPExplorerAE
{
    enum RecordCapability : unsigned { Grant = 1, Spawn = 2, Place = 4, Spell = 8, Perk = 16, Teleport = 32, Outfit = 64, Constructed = 128, Equip = 256, Quest = 512, Weather = 1024, Sound = 2048, Global = 4096 };
    struct RecordCategoryPolicy { std::string_view type; unsigned capabilities; };
    inline constexpr auto recordCategoryPolicies = std::to_array<RecordCategoryPolicy>({
        RecordCategoryPolicy{ "WEAP", Grant | Spawn | Equip }, { "ARMO", Grant | Spawn | Equip }, { "AMMO", Grant | Spawn },
        { "MISC", Grant | Spawn }, { "ALCH", Grant | Spawn }, { "BOOK", Grant | Spawn }, { "KEYM", Grant | Spawn },
        { "NOTE", Grant | Spawn }, { "INGR", Grant | Spawn }, { "CMPO", Grant | Spawn }, { "OMOD", Grant | Spawn },
        { "NPC_", Spawn }, { "LVLN", Spawn }, { "ACTI", Spawn | Place }, { "CONT", Spawn | Place },
        { "STAT", Spawn | Place }, { "FURN", Spawn | Place }, { "LIGH", Spawn | Place }, { "FLOR", Spawn | Place },
        { "TREE", Spawn | Place }, { "SPEL", Spell }, { "PERK", Perk }, { "CELL", Teleport },
        { "OTFT", Outfit }, { "COBJ", Constructed }, { "QUST", Quest }, { "WTHR", Weather },
        { "SOUN", Sound }, { "SNDR", Sound }, { "GLOB", Global }
    });

    inline bool SupportsRecordAction(std::string_view type, ActionKind kind)
    {
        unsigned capability{};
        switch (kind) {
        case ActionKind::Give: case ActionKind::GiveWithAmmo: capability = Grant; break;
        case ActionKind::Spawn: capability = Spawn; break;
        case ActionKind::Place: capability = Place; break;
        case ActionKind::AddSpell: case ActionKind::RemoveSpell: capability = Spell; break;
        case ActionKind::AddPerk: case ActionKind::RemovePerk: capability = Perk; break;
        case ActionKind::Teleport: capability = Teleport; break;
        case ActionKind::Outfit: capability = Outfit; break;
        case ActionKind::ConstructedItem: capability = Constructed; break;
        case ActionKind::Equip: capability = Equip; break;
        case ActionKind::StartQuest: case ActionKind::CompleteQuest: capability = Quest; break;
        case ActionKind::SetWeather: capability = Weather; break;
        case ActionKind::PlaySound: capability = Sound; break;
        case ActionKind::SetGlobal: capability = Global; break;
        default: return false;
        }
        for (const auto& policy : recordCategoryPolicies) if (policy.type == type) return (policy.capabilities & capability) != 0;
        return false;
    }

    inline std::optional<ActionRequest> PrepareRecordAction(ActionKind kind, const FormEntry& entry,
        std::uint64_t session, bool ready, std::uint32_t count = 1)
    {
        if (!ready || !session || !entry.formID || entry.isDeleted || !SupportsRecordAction(entry.category, kind) ||
            ((kind == ActionKind::Teleport || kind == ActionKind::PlaySound || kind == ActionKind::SetGlobal) && entry.editorID.empty())) return {};
        ActionRequest request{ .kind = kind, .formID = entry.formID, .count = count, .session = session };
        return ActionQueue::Valid(request) ? std::optional{ request } : std::nullopt;
    }
    inline bool CanTeleportRecord(const FormEntry& entry)
    {
        return entry.formID && entry.category == "CELL" && !entry.isDeleted && !entry.editorID.empty();
    }

    inline std::optional<CatalogResult> SelectGrantRecords(std::shared_ptr<const CatalogSnapshot> snapshot, std::span<const std::uint32_t> ids)
    {
        if (!snapshot || !snapshot->ready || ids.empty()) return {};
        CatalogResult selected{ std::move(snapshot), 0, {} };
        std::unordered_set<std::uint32_t> seen;
        for (const auto id : ids) {
            const auto* record = selected.snapshot->Find(id);
            if (!record || record->isDeleted || !SupportsRecordAction(record->category, ActionKind::Give)) return {};
            if (seen.insert(id).second) selected.order.push_back(selected.snapshot->byID.at(id));
        }
        return selected;
    }

    inline std::optional<ActionRequest> PrepareTeleport(const FormEntry& entry, std::uint64_t session, bool ready)
    {
        return PrepareRecordAction(ActionKind::Teleport, entry, session, ready);
    }
}
