#pragma once

namespace ESPExplorerAE
{
    enum class OverlayState { Closed, Open, Suspended };

    struct OverlaySettings
    {
        bool pause{};
        bool hideHUD{};
        bool godMode{};
    };

    struct OverlayFacts
    {
        bool visible{};
        bool focused{ true };
        bool modal{};
        bool keyboardDialog{};
        bool rendererReady{};
        bool blocked{};
        bool worldReady{};
        bool powerArmorHUD{};
        bool mainMenu{};
    };

    struct OverlayDecision
    {
        OverlayState state{ OverlayState::Closed };
        bool render{};
        bool capture{};
        bool pause{};
        bool hideHUD{};
        bool godMode{};
    };

    inline OverlayDecision DecideOverlay(const OverlayFacts& facts, const OverlaySettings& settings)
    {
        if (!facts.visible) return {};
        if (!facts.rendererReady || !facts.focused || facts.blocked || facts.modal || facts.keyboardDialog) {
            return { .state = OverlayState::Suspended };
        }
        return { .state = OverlayState::Open, .render = true, .capture = true,
            .pause = facts.worldReady && settings.pause,
            .hideHUD = facts.worldReady && settings.hideHUD && !facts.powerArmorHUD,
            .godMode = facts.worldReady && settings.godMode };
    }

    // A temporary override is applied once per activation and relinquished if
    // someone changes the observed value. An asynchronous write is not treated
    // as applied until its value is observed. Identical writes by another owner
    // cannot be distinguished without cooperation from that owner.
    template <class T>
    class TemporaryOverride
    {
    public:
        explicit TemporaryOverride(T value) : value(value) {}

        template <class Write>
        void Update(bool active, T current, Write&& write)
        {
            if (phase == Phase::Restoring) {
                if (current == value) return; // Restore was admitted but has not been observed.
                phase = Phase::Idle;
            }
            if (phase == Phase::Pending && current == value) phase = Phase::Owned;
            if (!active) {
                if (phase == Phase::Pending) return;
                if (phase == Phase::Owned && current == value) {
                    if (write(previous)) phase = Phase::Restoring;
                    return;
                }
                phase = Phase::Idle;
                return;
            }
            if (phase == Phase::Idle) {
                previous = current;
                if (current == value) phase = Phase::Preexisting;
                else if (write(value)) phase = Phase::Pending;
            } else if (phase == Phase::Owned && current != value) {
                phase = Phase::Relinquished;
            }
        }

        void Forget() { phase = Phase::Idle; }
        bool Engaged() const { return phase != Phase::Idle; }
        bool RequiresRestoration() const { return phase == Phase::Pending || phase == Phase::Owned || phase == Phase::Restoring; }

    private:
        enum class Phase { Idle, Preexisting, Pending, Owned, Restoring, Relinquished };
        Phase phase{ Phase::Idle };
        T value;
        T previous{};
    };
}
