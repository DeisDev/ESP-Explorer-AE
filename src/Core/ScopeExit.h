#pragma once

#include <utility>

namespace ESPExplorerAE
{
    template <class Cleanup>
    class ScopeExit
    {
    public:
        explicit ScopeExit(Cleanup cleanup) : cleanup(std::move(cleanup)) {}
        ScopeExit(const ScopeExit&) = delete;
        ScopeExit& operator=(const ScopeExit&) = delete;
        ~ScopeExit() noexcept { if (active) cleanup(); }
        void Release() noexcept { active = false; }

    private:
        Cleanup cleanup;
        bool active{ true };
    };
}
