#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace ESPExplorerAE::ArchiveRegistry
{
    // Narrow borrowed views of the loader's archive-name -> stream table.
    // This table uses a different allocator/layout from the pinned BSTHashMap.
    // Only access under its engine lock after ArchiveReader's code checks.
    struct Entry
    {
        const void* name{};  // BSFixedString storage, borrowed
        const void* stream{};
        const Entry* next{}; // null marks a free slot; never follow the chain
    };
    struct View
    {
        std::uint32_t unknown{};
        std::uint32_t capacity{};
        std::uint32_t free{};
        std::uint32_t lastFree{};
        const Entry* sentinel{};
        std::uint64_t allocator{};
        const Entry* entries{};
    };
    static_assert(sizeof(Entry) == 0x18 && offsetof(Entry, next) == 0x10);
    static_assert(sizeof(View) == 0x28 && offsetof(View, entries) == 0x20);

    template <class ReadName>
    std::optional<std::vector<std::string>> Capture(const View& view, ReadName readName)
    {
        constexpr std::size_t MaxSlots = 65536;
        constexpr std::size_t MaxName = 1024;
        if (view.capacity > MaxSlots || view.free > view.capacity ||
            (view.capacity && (!std::has_single_bit(view.capacity) || !view.entries || !view.sentinel))) return {};
        std::vector<std::string> names;
        names.reserve(view.capacity - view.free);
        std::size_t occupied{};
        for (std::uint32_t i = 0; i < view.capacity; ++i) {
            const auto& entry = view.entries[i];
            if (!entry.next) continue;
            ++occupied;
            if (!entry.name || !entry.stream) return {};
            const auto* name = readName(entry);
            if (!name) return {};
            std::size_t length{};
            while (length <= MaxName && name[length]) ++length;
            if (!length || length > MaxName) return {};
            names.emplace_back(name, length);
        }
        if (occupied != view.capacity - view.free) return {};
        std::ranges::sort(names);
        return names;
    }

    template <class ReadStream>
    bool AppendStreams(std::vector<std::string>& names, std::span<const void* const> streams,
        std::unordered_set<const void*>& seen, ReadStream readStream)
    {
        for (const auto* stream : streams) {
            if (!stream || !seen.insert(stream).second) continue;
            auto name = readStream(stream);
            if (!name) return false;
            // A retained but closed stream is no longer a loaded archive.
            if (!name->empty()) names.push_back(std::move(*name));
        }
        return true;
    }
}
