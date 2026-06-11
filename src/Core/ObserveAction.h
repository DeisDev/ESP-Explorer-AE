#pragma once

#include "Core/Actions.h"

namespace ESPExplorerAE
{
    // Nullopt means the reader cannot establish complete state. Dispatch alone
    // never proves an effect or an exact inverse.
    template <class Read, class Dispatch, class IsCurrent>
    ActionEffect ObserveAction(ActionEffect effect, Read&& read, Dispatch&& dispatch,
        IsCurrent&& isCurrent, std::optional<std::int64_t> desired = std::nullopt)
    {
        try {
            if (!isCurrent()) return effect;
            effect.before = read();
            if (desired && effect.before == desired) {
                effect.after = effect.before;
                effect.status = ActionStatus::NoChange;
                return effect;
            }
            if (!isCurrent() || !dispatch()) return effect;
            effect.status = ActionStatus::Dispatched;
            if (!isCurrent()) return effect;
            effect.after = read();
            if (effect.before && effect.after && effect.before != effect.after) {
                effect.status = ActionStatus::VerifiedChanged;
            }
        } catch (...) {
            effect.status = ActionStatus::Failed;
        }
        return effect;
    }
}
