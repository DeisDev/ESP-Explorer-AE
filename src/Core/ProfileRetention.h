#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>

namespace ESPExplorerAE
{
    enum class ProfileSnapshotKind { Catalog, Inventory };
    class ProfileRetention
    {
    public:
        static constexpr std::size_t Capacity = 512;
        struct Count { std::uint64_t objects{}, estimatedBytes{}; };
        struct Snapshot { std::array<Count, 2> live{}; std::uint64_t dropped{}; };
        bool Track(ProfileSnapshotKind kind, std::shared_ptr<const void> owner, std::size_t estimatedBytes)
        {
            if (!owner) return false;
            std::lock_guard lock(mutex);
            Entry* available{};
            for (auto& entry : entries) {
                if (const auto existing = entry.owner.lock(); existing && existing.get() == owner.get()) {
                    entry.bytes = estimatedBytes;
                    return true;
                }
                if (!available && entry.owner.expired()) available = &entry;
            }
            if (!available) { ++dropped; return false; }
            *available = { kind, std::move(owner), estimatedBytes };
            return true;
        }
        Snapshot Read() const
        {
            std::lock_guard lock(mutex);
            Snapshot result{ .dropped = dropped };
            for (const auto& entry : entries) if (!entry.owner.expired()) {
                auto& count = result.live[static_cast<std::size_t>(entry.kind)];
                ++count.objects;
                count.estimatedBytes += entry.bytes;
            }
            return result;
        }
        void Clear() { std::lock_guard lock(mutex); entries = {}; dropped = 0; }
    private:
        struct Entry { ProfileSnapshotKind kind{}; std::weak_ptr<const void> owner; std::size_t bytes{}; };
        mutable std::mutex mutex;
        std::array<Entry, Capacity> entries;
        std::uint64_t dropped{};
    };
}
