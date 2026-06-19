#pragma once

#include "Core/SettingsValidation.h"

namespace ESPExplorerAE
{
    enum class SettingsEffect { SelectFont, UpdateLogging, RequestSave, SaveNow, ReloadLanguage, RebuildFonts };

    template <class ApplyEffect>
    bool CommitSettings(Settings& current, Settings next, ApplyEffect&& apply)
    {
        ValidateSettings(next);
        if (current == next) return false;
        const bool font = current.fontSize != next.fontSize;
        const bool logging = current.debugLogging != next.debugLogging;
        const bool language = current.language != next.language;
        current = std::move(next);
        if (font) apply(SettingsEffect::SelectFont);
        if (logging) apply(SettingsEffect::UpdateLogging);
        if (language) {
            apply(SettingsEffect::SaveNow);
            apply(SettingsEffect::ReloadLanguage);
            apply(SettingsEffect::RebuildFonts);
        } else apply(SettingsEffect::RequestSave);
        return true;
    }
}
