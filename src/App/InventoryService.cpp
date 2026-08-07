#include "App/InventoryService.h"
#include "App/Profiler.h"
#include "Core/Profiling.h"
#include "Core/InventoryRefresh.h"
#include "Game/InventoryRuntime.h"
#include "pch.h"

#include <chrono>
#include <mutex>

namespace ESPExplorerAE
{
    namespace
    {
        std::mutex inventoryMutex;
        InventoryRefresh refresh;
        bool resetRuntime{};
        InventoryRefresh::Milliseconds Now()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }

    std::shared_ptr<const InventorySnapshot> InventoryService::Request(std::uint64_t session, bool force)
    {
        std::lock_guard lock(inventoryMutex);
        return refresh.Request(session, Now(), force);
    }

    void InventoryService::ResetSession()
    {
        std::lock_guard lock(inventoryMutex);
        refresh.Clear();
        resetRuntime = true; // Engine handle release is deferred to the game task.
    }

    void InventoryService::Invalidate()
    {
        std::lock_guard lock(inventoryMutex);
        refresh.Invalidate();
    }

    void InventoryService::Pump(std::uint64_t session, bool ready, bool pendingActions)
    {
        std::optional<InventoryRefresh::Ticket> ticket;
        bool release{};
        {
            std::lock_guard lock(inventoryMutex);
            release = std::exchange(resetRuntime, false);
            if ((!ready || (!refresh.Requested(Now()) && !pendingActions)) && refresh.HasSnapshot()) {
                refresh.Clear();
                release = true;
            }
            ticket = refresh.Begin(session, ready, Now());
        }
        if (release) InventoryRuntime::Reset();
        if (!ticket) return;
        std::shared_ptr<const InventorySnapshot> value;
        try {
            const ProfileScope profileScope(ProfileMetric::InventoryCapture);
            value = InventoryRuntime::Capture(session);
        }
        catch (const std::exception& error) { REX::WARN("Inventory capture failed: {}", error.what()); }
        catch (...) { REX::WARN("Inventory capture failed"); }
        Profiler::Track(value);
        std::lock_guard lock(inventoryMutex);
        refresh.Finish(*ticket, std::move(value), Now());
    }
}
