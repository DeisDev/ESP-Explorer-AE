#pragma once

#include "Core/Actions.h"
#include <functional>

namespace ESPExplorerAE
{
    // Game-task-only inventory boundary. Engine references stay in its registry;
    // callers receive immutable values and session-scoped opaque tokens.
    class InventoryRuntime
    {
    public:
        static std::shared_ptr<const InventorySnapshot> Capture(std::uint64_t session);
        static ActionOutcome Execute(const ActionRequest& request, const std::function<bool()>& isCurrent);
        static void Reset();
    };
}
