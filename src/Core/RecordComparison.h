#pragma once
#include "Core/CatalogSnapshot.h"
#include "Core/InventorySnapshot.h"
#include <variant>
#include <array>
#include <cmath>

namespace ESPExplorerAE
{
    enum class ComparisonField { Name, Type, Identity, Damage, Armor, Weight, Value, Ammo, Keywords, Attachments, Enchantments, Condition, Quantity, Equipped, Count };
    struct ComparisonReference
    {
        std::uint32_t formID{};
        std::uint8_t slot{};
        std::uint8_t rank{};
        bool disabled{};
        auto operator<=>(const ComparisonReference&) const = default;
    };
    using ComparisonReferences = std::vector<ComparisonReference>;
    using ComparisonValue = std::variant<std::monostate, double, std::string, bool, ComparisonReferences>;
    struct ComparisonRecord
    {
        std::uint32_t formID{};
        std::string name;
        std::uint64_t session{};
        std::uint64_t generation{};
        std::uint64_t token{};
        std::array<ComparisonValue, static_cast<std::size_t>(ComparisonField::Count)> values;
        bool instance{};
        ComparisonValue& At(ComparisonField field) { return values[static_cast<std::size_t>(field)]; }
        const ComparisonValue& At(ComparisonField field) const { return values[static_cast<std::size_t>(field)]; }
    };
    inline ComparisonValue ComparisonNumber(std::optional<double> value) { return value && std::isfinite(*value) ? ComparisonValue(*value) : ComparisonValue{}; }
    inline ComparisonReferences ComparisonIDs(const std::vector<std::uint32_t>& ids)
    {
        ComparisonReferences result;
        for (auto id : ids) result.push_back({id});
        std::ranges::sort(result); result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
    inline ComparisonRecord CompareBaseRecord(const FormEntry& record, std::uint64_t session, std::uint64_t generation)
    {
        ComparisonRecord result{record.formID, record.name, session, generation};
        result.At(ComparisonField::Name) = record.name; result.At(ComparisonField::Type) = record.category;
        result.At(ComparisonField::Identity) = ComparisonReferences{{record.formID}};
        result.At(ComparisonField::Damage) = ComparisonNumber(record.baseDamage);
        result.At(ComparisonField::Armor) = ComparisonNumber(record.armorRating);
        result.At(ComparisonField::Weight) = ComparisonNumber(record.weight);
        result.At(ComparisonField::Value) = record.value ? ComparisonValue(static_cast<double>(*record.value)) : ComparisonValue{};
        if (record.category == "WEAP") result.At(ComparisonField::Ammo) = record.weaponAmmoID ? ComparisonReferences{{record.weaponAmmoID}} : ComparisonReferences{};
        result.At(ComparisonField::Keywords) = ComparisonIDs(record.keywordIDs);
        return result;
    }
    inline ComparisonRecord CompareInventoryRecord(const InventoryStack& stack, std::uint64_t session, std::uint64_t generation)
    {
        ComparisonRecord result{stack.formID, stack.name, session, generation, stack.token};
        result.instance = true;
        result.At(ComparisonField::Name) = stack.name; result.At(ComparisonField::Type) = stack.category;
        result.At(ComparisonField::Identity) = ComparisonReferences{{stack.formID}};
        if (stack.category == "WEAP") result.At(ComparisonField::Damage) = static_cast<double>(stack.damage);
        if (stack.category == "ARMO") result.At(ComparisonField::Armor) = static_cast<double>(stack.armorRating);
        result.At(ComparisonField::Weight) = ComparisonNumber(stack.weight); result.At(ComparisonField::Value) = static_cast<double>(stack.value);
        if (stack.keywordIDs) result.At(ComparisonField::Keywords) = ComparisonIDs(*stack.keywordIDs);
        ComparisonReferences mods;
        for (const auto& mod : stack.mods) mods.push_back({mod.formID, mod.attachIndex, mod.rank, mod.disabled});
        std::ranges::sort(mods); result.At(ComparisonField::Attachments) = std::move(mods);
        ComparisonReferences enchantments;
        for (const auto& enchantment : stack.enchantments) enchantments.push_back({enchantment.formID});
        std::ranges::sort(enchantments); result.At(ComparisonField::Enchantments) = std::move(enchantments);
        if (stack.healthPercent >= 0) result.At(ComparisonField::Condition) = ComparisonNumber(stack.healthPercent * 100.0);
        result.At(ComparisonField::Quantity) = static_cast<double>(stack.count); result.At(ComparisonField::Equipped) = stack.isEquipped;
        return result;
    }
    inline std::vector<ComparisonField> ComparisonRows(const ComparisonRecord& a, const ComparisonRecord& b, bool differencesOnly)
    {
        std::vector<ComparisonField> result;
        for (std::size_t i = 0; i < a.values.size(); ++i) if (!differencesOnly || a.values[i] != b.values[i]) result.push_back(static_cast<ComparisonField>(i));
        return result;
    }
}
