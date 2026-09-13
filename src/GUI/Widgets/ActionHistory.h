#pragma once

#include "Core/Actions.h"
#include <functional>

namespace ESPExplorerAE
{
    struct ActionHistoryEntry
    {
        std::uint64_t id{};
        std::string description;
        std::string status;
        std::string details;
        ActionStatus result{ ActionStatus::Dispatched };
        std::uint64_t groupID{};
        std::string groupName;
        std::uint32_t target{};
    };

    using ActionHistoryLocalize = std::function<const char*(std::string_view, std::string_view, const char*)>;

    // Receipts remain application-owned; the presentation owns its localized
    // strings and never retains pointers into receipts or locale snapshots.
    std::vector<ActionHistoryEntry> FormatActionHistory(std::span<const ActionReceipt> receipts, const ActionHistoryLocalize& localize);
}
