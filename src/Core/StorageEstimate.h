#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/InventorySnapshot.h"

namespace ESPExplorerAE::StorageEstimate
{
    // Capacity-based estimates: allocator headers, regex internals and engine
    // allocations are unknown. Inline string storage is already in sizeof(T).
    inline std::size_t Heap(const std::string& value)
    {
        const auto object = reinterpret_cast<std::uintptr_t>(&value);
        const auto data = reinterpret_cast<std::uintptr_t>(value.data());
        return data >= object && data < object + sizeof(value) ? 0 : value.capacity() + 1;
    }
    template <class T> std::size_t Storage(const std::vector<T>& values) { return values.capacity() * sizeof(T); }
    inline std::size_t Strings(const std::vector<std::string>& values)
    {
        auto bytes = Storage(values);
        for (const auto& value : values) bytes += Heap(value);
        return bytes;
    }
    template <class Map> std::size_t Hash(const Map& values)
    {
        return values.bucket_count() * sizeof(void*) + values.size() * (sizeof(typename Map::value_type) + sizeof(void*));
    }
    inline std::size_t Catalog(const CatalogSnapshot& value)
    {
        auto bytes = sizeof(value) + Storage(value.records) + Storage(value.plugins) + Strings(value.availableKeywords) +
            Hash(value.byID) + Hash(value.byType) + Hash(value.byPlugin) + Hash(value.runtimeReferenceCounts) + Hash(value.incomingRelationships);
        for (const auto& [id, links] : value.incomingRelationships) bytes += Storage(links);
        for (const auto& record : value.records) bytes += Heap(record.name) + Heap(record.category) + Heap(record.sourcePlugin) +
            Heap(record.weaponAmmoName) + Heap(record.worldspace) + Heap(record.race) + Heap(record.factions) + Heap(record.editorID) + Strings(record.keywords) + Strings(record.factionNames) + Storage(record.keywordIDs) + Storage(record.relationships);
        for (const auto& plugin : value.plugins) bytes += Heap(plugin.filename) + Heap(plugin.type) + Heap(plugin.formIDPrefix) +
            Strings(plugin.masters) + Strings(plugin.missingMasters);
        for (const auto& [key, rows] : value.byType) bytes += Heap(key) + Storage(rows);
        for (const auto& [key, rows] : value.byPlugin) bytes += Heap(key) + Storage(rows);
        return bytes;
    }
    inline std::size_t Inventory(const InventorySnapshot& value)
    {
        auto bytes = sizeof(value) + Storage(value.stacks) + Storage(value.groups) + Hash(value.byToken);
        for (const auto& stack : value.stacks) {
            bytes += Heap(stack.name) + Heap(stack.category) + Heap(stack.sourcePlugin) + Heap(stack.legendaryName) +
                Storage(stack.mods) + Storage(stack.enchantments);
            if (stack.keywordIDs) bytes += Storage(*stack.keywordIDs);
            for (const auto& mod : stack.mods) bytes += Heap(mod.slotLabel) + Heap(mod.name);
            for (const auto& enchantment : stack.enchantments) bytes += Heap(enchantment.name);
        }
        for (const auto& group : value.groups) bytes += Storage(group.stacks);
        return bytes;
    }
}
