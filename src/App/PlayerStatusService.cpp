#include "pch.h"
#include "App/PlayerStatusService.h"
#include "Game/PlayerStatusReader.h"
#include <chrono>
#include <mutex>

namespace ESPExplorerAE
{
    namespace
    {
        std::mutex statusMutex;
        PlayerStatusState status;
        PlayerStatusState::Milliseconds retryAfter{};
        PlayerStatusState::Milliseconds Now()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }

    PlayerStatus PlayerStatusService::Request(std::uint64_t session)
    {
        std::lock_guard lock(statusMutex);
        return status.Request(session, Now());
    }

    void PlayerStatusService::ResetSession()
    {
        std::lock_guard lock(statusMutex);
        status.Reset();
        retryAfter = 0;
    }

    void PlayerStatusService::Pump(std::uint64_t session, bool ready)
    {
        std::optional<PlayerStatusState::Ticket> ticket;
        {
            std::lock_guard lock(statusMutex);
            ticket = status.Begin(session, ready && Now() >= retryAfter, Now());
        }
        if (!ticket) return;
        PlayerStatus next{ .session = session };
        bool failed{};
        try { next = ReadPlayerStatus(session); }
        catch (const std::exception& error) { failed = true; REX::WARN("Player status unavailable: {}", error.what()); }
        catch (...) { failed = true; REX::WARN("Player status unavailable"); }
        std::lock_guard lock(statusMutex);
        const auto now = Now();
        if (status.Finish(*ticket, next, now) && failed) retryAfter = now + 2000;
    }
}
