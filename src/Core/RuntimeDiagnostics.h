#pragma once

#include "Core/CatalogSnapshot.h"

namespace ESPExplorerAE
{
    // A runtime FormID identifies the loaded full/light namespace that owns the
    // ID. It does not identify a winning override or enumerate on-disk records.
    inline void PopulateRuntimeDiagnostics(CatalogSnapshot& catalog)
    {
        std::unordered_map<std::string, std::size_t> byName;
        std::unordered_map<std::uint32_t, std::optional<std::size_t>> origins;
        for (std::size_t index = 0; index < catalog.plugins.size(); ++index) {
            auto& plugin = catalog.plugins[index];
            plugin.runtimeSourceRecords = 0;
            plugin.runtimeOriginRecords = 0;
            plugin.overrideCount.reset();
            plugin.overriddenByOthersCount.reset();
            byName.emplace(plugin.filename, index);
            if ((!plugin.isLight && plugin.loadOrder >= 0xFE) || (plugin.isLight && plugin.lightOrder > 0xFFF)) continue;
            const auto prefix = plugin.isLight ? 0xFE000000u | (plugin.lightOrder << 12) : plugin.loadOrder << 24;
            const auto [it, inserted] = origins.emplace(prefix, index);
            if (!inserted) it->second.reset(); // Ambiguous namespaces cannot establish an origin.
        }
        for (const auto& record : catalog.records) {
            if (const auto source = byName.find(record.sourcePlugin); source != byName.end()) ++catalog.plugins[source->second].runtimeSourceRecords;
            if (!record.formID || (record.formID >> 24) == 0xFF) continue;
            const auto prefix = record.formID & (((record.formID >> 24) == 0xFE) ? 0xFFFFF000u : 0xFF000000u);
            if (const auto origin = origins.find(prefix); origin != origins.end() && origin->second) ++catalog.plugins[*origin->second].runtimeOriginRecords;
        }
    }
}
