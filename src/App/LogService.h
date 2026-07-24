#pragma once
#include "Core/Logs.h"
#include <memory>
#include <string_view>

namespace ESPExplorerAE
{
    class LogService
    {
    public:
        // Render-owner methods; no filesystem work occurs in the detached view.
        static std::shared_ptr<const LogSnapshot> Read();
        static void Apply(const LogRequests& requests, std::string_view logLabel, std::string_view allLabel);
        static void Reset();
    };
}
