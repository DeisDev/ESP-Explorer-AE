#pragma once

#include "Core/RecordSelection.h"
#include <optional>
#include <string>

namespace ESPExplorerAE
{
    struct TreeRow
    {
        std::string section;
        std::uint32_t formID{};
        bool operator==(const TreeRow&) const = default;
    };

    struct TreeSelection
    {
        RecordSelection records;
        std::optional<TreeRow> anchor;

        void Clear() { records.Clear(); anchor.reset(); }
        void Single(const TreeRow& row) { records.Single(row.formID); anchor = row; }
        void Click(std::span<const TreeRow> fullOrder, const TreeRow& row, bool control, bool shift)
        {
            const auto target = std::ranges::find(fullOrder, row);
            if (target == fullOrder.end()) return;
            const auto start = anchor ? std::ranges::find(fullOrder, *anchor) : fullOrder.end();
            if (shift && start != fullOrder.end()) {
                if (!control) records.selected.clear();
                const auto [first, last] = std::minmax(start, target);
                for (auto it = first; it != last + 1; ++it) records.selected.insert(it->formID);
            } else if (control) {
                if (!records.selected.erase(row.formID)) records.selected.insert(row.formID);
                anchor = row;
            } else Single(row);
            records.active = row.formID;
        }
        void Reconcile(const std::unordered_set<std::uint32_t>& eligible)
        {
            std::erase_if(records.selected, [&](auto id) { return !eligible.contains(id); });
            if (!records.selected.contains(records.active)) records.active = 0;
            if (anchor && !eligible.contains(anchor->formID)) anchor.reset();
        }
    };
}
