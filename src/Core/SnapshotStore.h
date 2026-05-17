#pragma once

#include <memory>
#include <mutex>

namespace ESPExplorerAE
{
    // Publication only holds the mutex while exchanging ownership. Readers never
    // hold a store lock while using a snapshot, including during another publish.
    template <class Snapshot>
    class SnapshotStore
    {
    public:
        std::shared_ptr<const Snapshot> Read() const
        {
            std::lock_guard lock(mutex);
            return current;
        }

        void Publish(std::shared_ptr<const Snapshot> next)
        {
            std::lock_guard lock(mutex);
            current = std::move(next);
        }

    private:
        mutable std::mutex mutex;
        std::shared_ptr<const Snapshot> current{ std::make_shared<const Snapshot>() };
    };
}
