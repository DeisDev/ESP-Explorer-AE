#pragma once

#include "Core/FormEntry.h"
#include <array>
#include <cmath>

namespace ESPExplorerAE
{
    // Stable numeric identifiers are independent of column order and locale.
    enum class RecordColumn : int { FormID, Name, Source, Type, EditorID, Ammo, Damage, Weight, Value, Race, Essential, Unique, Protected, Interior, Worldspace, Count };
    struct RecordColumnLayout
    {
        float width{ 120.0f };
        int order{};
        bool visible{};
        bool operator==(const RecordColumnLayout&) const = default;
    };
    using RecordTableLayout = std::array<RecordColumnLayout, static_cast<std::size_t>(RecordColumn::Count)>;
    inline RecordTableLayout DefaultRecordColumns(bool technical = false)
    {
        RecordTableLayout result;
        constexpr std::array order{1, 3, 2, 4, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
        for (std::size_t index = 0; index < order.size(); ++index) {
            const auto id = order[index];
            result[id] = {id == 1 ? 260.0f : id == 2 ? 200.0f : 120.0f, static_cast<int>(index),
                id == 1 || id == 2 || id == 3 || (technical && (id == 0 || id == 4))};
        }
        return result;
    }
    inline std::optional<double> RecordColumnNumber(const FormEntry& record, int column)
    {
        switch (static_cast<RecordColumn>(column)) {
        case RecordColumn::Damage: return record.baseDamage && std::isfinite(*record.baseDamage) ? std::optional<double>(*record.baseDamage) : std::nullopt;
        case RecordColumn::Weight: return record.weight && std::isfinite(*record.weight) ? std::optional<double>(*record.weight) : std::nullopt;
        case RecordColumn::Value: return record.value ? std::optional<double>(static_cast<double>(*record.value)) : std::nullopt;
        case RecordColumn::Essential: if (record.hasNPCData) return record.npcEssential; break;
        case RecordColumn::Unique: if (record.hasNPCData) return record.npcUnique; break;
        case RecordColumn::Protected: if (record.hasNPCData) return record.npcProtected; break;
        case RecordColumn::Interior: if (record.cellInterior) return *record.cellInterior; break;
        default: break;
        }
        return {};
    }
    inline std::string_view RecordColumnText(const FormEntry& record, int column)
    {
        switch (static_cast<RecordColumn>(column)) {
        case RecordColumn::Source: return record.sourcePlugin;
        case RecordColumn::Type: return record.category;
        case RecordColumn::EditorID: return record.editorID;
        case RecordColumn::Ammo: return record.weaponAmmoName;
        case RecordColumn::Race: return record.race;
        case RecordColumn::Worldspace: return record.worldspace;
        default: return record.name;
        }
    }
    inline bool RecordColumnNumeric(int column) { return (column >= 6 && column <= 8) || (column >= 10 && column <= 13); }
}
