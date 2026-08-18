#include "pch.h"
#include "App/SettingsService.h"
#include "Config/Config.h"
#include "Config/ThemeStore.h"
#include "Core/SettingsValidation.h"
#include "Core/SettingsEffects.h"
#include "Game/SettingsReader.h"
#include "Input/GamepadInput.h"
#include "Localization/FontManager.h"
#include "Localization/Language.h"
#include <REL/Version.h>
#include <spdlog/spdlog.h>

namespace ESPExplorerAE
{
    namespace { void ThemeDiagnostic(std::string issue) { REX::WARN("{}", issue); } }

    SettingsResources SettingsService::Read()
    {
        static const auto languages = std::make_shared<const std::vector<LanguageDefinition>>(Language::ListAvailableLanguages());
        static const auto gameVersion = [] {
            const auto version = REL::GetFileVersion(std::string_view("Fallout4.exe"));
            return version ? version->string() : std::string{};
        }();
        static const auto modVersion = F4SE::GetPluginVersion().string();
        static std::uint32_t namedKey{};
        static std::string keyName;
        const auto key = Config::Get().toggleKey;
        if (namedKey != key) {
            namedKey = key;
            const auto scanCode = MapVirtualKeyA(key, MAPVK_VK_TO_VSC);
            char name[128]{};
            const auto keyData = static_cast<LONG>(static_cast<LPARAM>(scanCode) << 16);
            keyName = GetKeyNameTextA(keyData, name, static_cast<int>(std::size(name))) > 0 ? std::string(name) : std::to_string(key);
        }
        return { ThemeStore::Snapshot(ThemeDiagnostic), languages, SettingsReader::Color(), keyName, gameVersion, modVersion, GamepadInput::IsGamepadConnected() };
    }

    void SettingsService::Apply(std::optional<Settings> value, bool reloadThemes)
    {
        if (!value && !reloadThemes) return;
        const auto before = Config::Get();
        auto next = value ? std::move(*value) : before;
        ValidateSettings(next);
        if (reloadThemes) {
            ThemeStore::ReloadAvailableThemes(ThemeDiagnostic);
            if (const auto* selected = FindTheme(*ThemeStore::Snapshot(ThemeDiagnostic), next.themePresetId)) ApplyTheme(next, *selected);
        }
        if (next.syncPipboyColor) ApplyPipboyColor(next, SettingsReader::Color());
        CommitSettings(Config::GetMutable(), std::move(next), [](SettingsEffect effect) {
            const auto& settings = Config::Get();
            switch (effect) {
            case SettingsEffect::SelectFont: FontManager::SetCurrentSizeIndex(FontManager::FindClosestSizeIndex(settings.fontSize)); break;
            case SettingsEffect::UpdateLogging: {
                const auto level = settings.debugLogging ? spdlog::level::debug : spdlog::level::info;
                spdlog::set_level(level);
                spdlog::default_logger()->flush_on(level);
                break;
            }
            case SettingsEffect::RequestSave: Config::RequestSave(); break;
            case SettingsEffect::SaveNow: Config::Save(); break;
            case SettingsEffect::ReloadLanguage: Language::Load(settings.language, [](std::string message) { REX::WARN("{}", message); }); break;
            case SettingsEffect::RebuildFonts: FontManager::RequestLanguageRebuild(); break;
            }
        });
    }

    void SettingsService::PumpRender()
    {
        static std::optional<PipboyColor> previous;
        auto& settings = Config::GetMutable();
        if (!settings.syncPipboyColor) { previous.reset(); return; }
        const auto color = SettingsReader::Color();
        if (previous && *previous == color) return;
        previous = color;
        if (ApplyPipboyColor(settings, color)) Config::RequestSave();
    }

    void SettingsService::OpenPage(SettingsPage page)
    {
        const char* url{};
        switch (page) {
        case SettingsPage::NexusMods: url = "https://www.nexusmods.com/fallout4/mods/102223"; break;
        case SettingsPage::GitHub: url = "https://github.com/DeisDev/ESP-Explorer-AE"; break;
        case SettingsPage::BugReport: url = "https://www.nexusmods.com/fallout4/mods/102223?tab=bugs"; break;
        case SettingsPage::Donations: url = "https://buymeacoffee.com/DeisDev"; break;
        }
        if (url) ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }
}
