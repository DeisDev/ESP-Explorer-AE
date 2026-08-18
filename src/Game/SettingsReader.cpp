#include "pch.h"
#include "Game/SettingsReader.h"
#include <RE/S/Setting.h>
#include <chrono>
#include <mutex>

namespace ESPExplorerAE
{
    namespace { std::mutex colorMutex; PipboyColor color; }

    void SettingsReader::Pump()
    {
        static std::chrono::steady_clock::time_point nextRead;
        const auto now = std::chrono::steady_clock::now();
        if (now < nextRead) return;
        nextRead = now + std::chrono::seconds(1);
        PipboyColor next;
        const auto read = [](std::string_view name, float& value) {
            const auto* setting = RE::GetINISetting(name);
            if (!setting || setting->GetType() != RE::Setting::SETTING_TYPE::kFloat) return false;
            value = setting->GetFloat();
            return std::isfinite(value);
        };
        next.valid = read("fPipboyEffectColorR:Pipboy", next.r) && read("fPipboyEffectColorG:Pipboy", next.g) && read("fPipboyEffectColorB:Pipboy", next.b);
        if (!next.valid) next = {};
        std::lock_guard lock(colorMutex);
        color = next;
    }
    PipboyColor SettingsReader::Color()
    {
        std::lock_guard lock(colorMutex);
        return color;
    }
}
