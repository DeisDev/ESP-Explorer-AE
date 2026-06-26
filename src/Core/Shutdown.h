#pragma once

#include "Core/ScopeExit.h"
#include <atomic>
#include <cstdint>

namespace ESPExplorerAE
{
    // A terminal process-lifetime transition. Game/render pumps each have one
    // owner; neither waits for the other. Completion requires observed cleanup,
    // successful persistence and conditional hook restoration, in that order.
    class ShutdownCoordinator
    {
    public:
        bool Request() { return !(flags.fetch_or(RequestedBit) & RequestedBit); }
        bool Requested() const { return (flags.load() & RequestedBit) != 0; }
        bool Complete() const { return (flags.load() & FinishedBit) != 0; }

        template <class Begin, class Poll>
        void PumpGame(Begin&& begin, Poll&& poll)
        {
            Pump(GameBit, gameStarted, begin, poll);
        }
        template <class Begin, class Poll>
        void PumpRender(Begin&& begin, Poll&& poll)
        {
            Pump(RenderBit, renderStarted, begin, poll);
        }
        template <class Restore>
        bool TryFinish(Restore&& restore)
        {
            const auto current = flags.load();
            if (current & FinishedBit) return true;
            if ((current & (GameBit | RenderBit)) != (GameBit | RenderBit) || restoring.test_and_set()) return false;
            ScopeExit unlock([&] { restoring.clear(); });
            if (Complete()) return true;
            if (!restore()) return false;
            flags.fetch_or(FinishedBit);
            return true;
        }

    private:
        template <class Begin, class Poll>
        void Pump(std::uint8_t bit, bool& started, Begin& begin, Poll& poll)
        {
            const auto current = flags.load();
            if (!(current & RequestedBit) || (current & bit)) return;
            if (!started) { begin(); started = true; }
            if (poll()) flags.fetch_or(bit);
        }
        static constexpr std::uint8_t RequestedBit = 1, GameBit = 2, RenderBit = 4, FinishedBit = 8;
        std::atomic<std::uint8_t> flags{};
        std::atomic_flag restoring{};
        bool gameStarted{};   // Accessed only by the game-task owner.
        bool renderStarted{}; // Accessed only by the render owner.
    };
}
