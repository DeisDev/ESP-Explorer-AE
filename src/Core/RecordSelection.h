#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_set>

namespace ESPExplorerAE
{
    template <class ID>
    struct OrderedSelection
    {
        std::unordered_set<ID> selected;
        ID anchor{};
        ID active{};

        void Clear() { selected.clear(); anchor = active = 0; }
        void Single(ID id) { selected = { id }; anchor = active = id; }
        void All(std::span<const ID> order)
        {
            selected = { order.begin(), order.end() };
            anchor = active = order.empty() ? 0 : order.front();
        }
        void Reconcile(std::span<const ID> order)
        {
            const std::unordered_set<ID> visible(order.begin(), order.end());
            std::erase_if(selected, [&](auto id) { return !visible.contains(id); });
            if (!visible.contains(anchor)) anchor = 0;
            if (!visible.contains(active)) active = 0;
        }
        void Click(std::span<const ID> order, ID id, bool control, bool shift)
        {
            const auto target = std::ranges::find(order, id);
            if (target == order.end()) return;
            const auto start = std::ranges::find(order, anchor);
            if (shift && start != order.end()) {
                if (!control) selected.clear();
                const auto [first, last] = std::minmax(start, target);
                selected.insert(first, last + 1);
            } else if (control) {
                if (!selected.erase(id)) selected.insert(id);
                anchor = id;
            } else {
                Single(id);
            }
            active = id;
        }
    };

    using RecordSelection = OrderedSelection<std::uint32_t>;
}
