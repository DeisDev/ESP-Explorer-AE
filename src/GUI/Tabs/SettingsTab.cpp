#include "GUI/Tabs/SettingsTab.h"

#include "Config/Config.h"
#include "GUI/MainWindow.h"
#include "GUI/ThemeManager.h"
#include "GUI/Widgets/MainWindowPopups.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Widgets/SharedUtils.h"
#include "Input/GamepadInput.h"
#include "Logging/Logger.h"
#include "Localization/FontManager.h"
#include "Localization/Language.h"

#include <imgui.h>

#include <cmath>
#include <RE/S/Setting.h>
#include <REL/Version.h>

namespace ESPExplorerAE
{
    namespace
    {
        bool waitingForToggleKey{ false };
        constexpr auto kNexusModsUrl = "https://www.nexusmods.com/fallout4/mods/102223";
        constexpr auto kNexusBugReportUrl = "https://www.nexusmods.com/fallout4/mods/102223?tab=bugs";
        constexpr auto kGitHubUrl = "https://github.com/DeisDev/ESP-Explorer-AE";
        constexpr auto kBuyMeACoffeeUrl = "https://buymeacoffee.com/DeisDev";

        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        }

        std::string KeyNameFromVK(std::uint32_t vk)
        {
            const auto scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            const LONG keyData = static_cast<LONG>(static_cast<LPARAM>(scanCode) << 16);

            char keyName[128]{};
            if (GetKeyNameTextA(keyData, keyName, static_cast<int>(std::size(keyName))) > 0) {
                return std::string(keyName);
            }

            return std::to_string(vk);
        }

        std::uint32_t ImGuiKeyToVK(ImGuiKey key)
        {
            if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
                return static_cast<std::uint32_t>('A' + (key - ImGuiKey_A));
            }

            if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
                return static_cast<std::uint32_t>('0' + (key - ImGuiKey_0));
            }

            if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) {
                return static_cast<std::uint32_t>(VK_F1 + (key - ImGuiKey_F1));
            }

            switch (key) {
            case ImGuiKey_Tab:
                return VK_TAB;
            case ImGuiKey_LeftArrow:
                return VK_LEFT;
            case ImGuiKey_RightArrow:
                return VK_RIGHT;
            case ImGuiKey_UpArrow:
                return VK_UP;
            case ImGuiKey_DownArrow:
                return VK_DOWN;
            case ImGuiKey_PageUp:
                return VK_PRIOR;
            case ImGuiKey_PageDown:
                return VK_NEXT;
            case ImGuiKey_Home:
                return VK_HOME;
            case ImGuiKey_End:
                return VK_END;
            case ImGuiKey_Insert:
                return VK_INSERT;
            case ImGuiKey_Delete:
                return VK_DELETE;
            case ImGuiKey_Backspace:
                return VK_BACK;
            case ImGuiKey_Space:
                return VK_SPACE;
            case ImGuiKey_Enter:
            case ImGuiKey_KeypadEnter:
                return VK_RETURN;
            case ImGuiKey_Escape:
                return VK_ESCAPE;
            case ImGuiKey_Apostrophe:
                return VK_OEM_7;
            case ImGuiKey_Comma:
                return VK_OEM_COMMA;
            case ImGuiKey_Minus:
                return VK_OEM_MINUS;
            case ImGuiKey_Period:
                return VK_OEM_PERIOD;
            case ImGuiKey_Slash:
                return VK_OEM_2;
            case ImGuiKey_Semicolon:
                return VK_OEM_1;
            case ImGuiKey_Equal:
                return VK_OEM_PLUS;
            case ImGuiKey_LeftBracket:
                return VK_OEM_4;
            case ImGuiKey_Backslash:
                return VK_OEM_5;
            case ImGuiKey_RightBracket:
                return VK_OEM_6;
            case ImGuiKey_GraveAccent:
                return VK_OEM_3;
            case ImGuiKey_CapsLock:
                return VK_CAPITAL;
            case ImGuiKey_ScrollLock:
                return VK_SCROLL;
            case ImGuiKey_NumLock:
                return VK_NUMLOCK;
            case ImGuiKey_PrintScreen:
                return VK_SNAPSHOT;
            case ImGuiKey_Pause:
                return VK_PAUSE;
            case ImGuiKey_Keypad0:
                return VK_NUMPAD0;
            case ImGuiKey_Keypad1:
                return VK_NUMPAD1;
            case ImGuiKey_Keypad2:
                return VK_NUMPAD2;
            case ImGuiKey_Keypad3:
                return VK_NUMPAD3;
            case ImGuiKey_Keypad4:
                return VK_NUMPAD4;
            case ImGuiKey_Keypad5:
                return VK_NUMPAD5;
            case ImGuiKey_Keypad6:
                return VK_NUMPAD6;
            case ImGuiKey_Keypad7:
                return VK_NUMPAD7;
            case ImGuiKey_Keypad8:
                return VK_NUMPAD8;
            case ImGuiKey_Keypad9:
                return VK_NUMPAD9;
            case ImGuiKey_KeypadDecimal:
                return VK_DECIMAL;
            case ImGuiKey_KeypadDivide:
                return VK_DIVIDE;
            case ImGuiKey_KeypadMultiply:
                return VK_MULTIPLY;
            case ImGuiKey_KeypadSubtract:
                return VK_SUBTRACT;
            case ImGuiKey_KeypadAdd:
                return VK_ADD;
            default:
                return 0;
            }
        }

        bool CaptureToggleKey(Settings& settings)
        {
            if (!waitingForToggleKey) {
                return false;
            }

            for (int keyIndex = ImGuiKey_NamedKey_BEGIN; keyIndex < ImGuiKey_NamedKey_END; ++keyIndex) {
                const auto key = static_cast<ImGuiKey>(keyIndex);
                if (ImGui::IsKeyPressed(key, false)) {
                    if (key == ImGuiKey_Escape) {
                        waitingForToggleKey = false;
                        return false;
                    }

                    const auto vk = ImGuiKeyToVK(key);
                    if (vk == 0 || vk == VK_TAB) {
                        continue;
                    }

                    settings.toggleKey = vk;
                    waitingForToggleKey = false;
                    return true;
                }
            }

            return false;
        }

        bool AutoPersist(bool changed)
        {
            if (!changed) {
                return false;
            }

            Config::RequestSave();
            return true;
        }

        constexpr auto kStartupTabLastActive = "__last__";

        struct StartupTabOption
        {
            const char* value;
            const char* section;
            const char* key;
            const char* fallback;
        };

        constexpr StartupTabOption kStartupTabOptions[] = {
            { kStartupTabLastActive, "Settings", "sStartupTabLastActive", "Last Active Tab" },
            { "Plugin Browser", "PluginBrowser", "sBrowserTab", "Plugin Browser" },
            { "Inventory", "Inventory", "sTabName", "Inventory" },
            { "Item Browser", "Items", "sBrowserTab", "Item Browser" },
            { "NPC Browser", "NPCs", "sBrowserTab", "NPC Browser" },
            { "Cell Browser", "Cells", "sBrowserTab", "Cell Browser" },
            { "Object Browser", "Objects", "sBrowserTab", "Object Browser" },
            { "Spells & Perks", "Spells", "sBrowserTab", "Spells & Perks" },
            { "Settings", "Settings", "sTabName", "Settings" },
            { "Logs", "Logs", "sTabName", "Logs" },
        };

        const StartupTabOption* FindStartupTabOption(std::string_view value)
        {
            for (const auto& option : kStartupTabOptions) {
                if (value == option.value) {
                    return &option;
                }
            }

            return &kStartupTabOptions[0];
        }

        void ResetVisualSettings(Settings& settings)
        {
            const Settings defaults{};
            settings.rememberWindowPos = defaults.rememberWindowPos;
            settings.fontSize = defaults.fontSize;
            settings.windowAlpha = defaults.windowAlpha;
            settings.themeAccentR = defaults.themeAccentR;
            settings.themeAccentG = defaults.themeAccentG;
            settings.themeAccentB = defaults.themeAccentB;
            settings.themeAccentA = defaults.themeAccentA;
            settings.themeWindowR = defaults.themeWindowR;
            settings.themeWindowG = defaults.themeWindowG;
            settings.themeWindowB = defaults.themeWindowB;
            settings.themeWindowA = defaults.themeWindowA;
            settings.themePanelR = defaults.themePanelR;
            settings.themePanelG = defaults.themePanelG;
            settings.themePanelB = defaults.themePanelB;
            settings.themePanelA = defaults.themePanelA;
            settings.syncPipboyColor = defaults.syncPipboyColor;
            settings.themePresetId = defaults.themePresetId;
        }

        const std::string& GetGameVersionText()
        {
            static const std::string cachedVersion = []() {
                if (const auto version = REL::GetFileVersion(std::string_view("Fallout4.exe")); version.has_value()) {
                    return version->string();
                }
                return std::string(L("General", "sUnknown", "Unknown"));
            }();

            return cachedVersion;
        }

        const std::string& GetModVersionText()
        {
            static const std::string cachedVersion = []() {
                return F4SE::GetPluginVersion().string();
            }();

            return cachedVersion;
        }

        const std::vector<Language::Definition>& GetAvailableLanguages()
        {
            static const std::vector<Language::Definition> cachedLanguages = Language::ListAvailableLanguages();
            return cachedLanguages;
        }

        std::string BuildLanguageLabel(const Language::Definition& language)
        {
            if (language.displayName.empty() || language.displayName == language.code) {
                return language.code;
            }

            return language.displayName + " (" + language.code + ")";
        }

        const std::string& GetToggleKeyText(std::uint32_t vk)
        {
            static std::uint32_t cachedVK = (std::numeric_limits<std::uint32_t>::max)();
            static std::string cachedName;

            if (cachedVK != vk) {
                cachedVK = vk;
                cachedName = KeyNameFromVK(vk);
            }

            return cachedName;
        }

        bool BeginSection(const char* id, const char* label)
        {
            ImGui::PushID(id);
            const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_OpenOnArrow);
            SharedUtils::DrawCurrentItemChrome(open, ImGui::IsItemHovered(), false, true);
            ImGui::PopID();
            return open;
        }

        const std::array<const char*, FontManager::kPresetCount>& GetFontSizeLabels()
        {
            static const std::array<const char*, FontManager::kPresetCount> labels = { "12 px", "14 px", "16 px", "18 px", "20 px", "22 px", "24 px" };
            return labels;
        }

        std::string GetThemeLabel(const ThemePreset& theme)
        {
            const auto fallback = theme.name.empty() ? theme.id : theme.name;
            if (!theme.nameKey.empty()) {
                return std::string(L("Settings", theme.nameKey, fallback.c_str()));
            }

            return fallback;
        }

        std::string GetColorPresetLabel(const Settings& settings)
        {
            if (settings.syncPipboyColor) {
                return std::string(L("Settings", "sThemePresetPipboySync", "Pip-Boy Synced"));
            }

            if (!settings.themePresetId.empty()) {
                const auto* selectedTheme = ThemeManager::FindThemeById(settings.themePresetId);
                const auto* matchingTheme = ThemeManager::FindMatchingTheme(settings);
                if (selectedTheme && selectedTheme == matchingTheme) {
                    return GetThemeLabel(*selectedTheme);
                }

                return std::string(L("Settings", "sThemePresetCustom", "Custom"));
            }

            if (const auto* matchingTheme = ThemeManager::FindMatchingTheme(settings)) {
                return GetThemeLabel(*matchingTheme);
            }

            return std::string(L("Settings", "sThemePresetCustom", "Custom"));
        }

        void ClearThemePreset(Settings& settings)
        {
            settings.themePresetId.clear();
        }

        bool TryReadPipboyColor(float& outR, float& outG, float& outB)
        {
            auto* settingR = RE::GetINISetting("fPipboyEffectColorR:Pipboy");
            auto* settingG = RE::GetINISetting("fPipboyEffectColorG:Pipboy");
            auto* settingB = RE::GetINISetting("fPipboyEffectColorB:Pipboy");

            if (!settingR || !settingG || !settingB) {
                return false;
            }

            if (settingR->GetType() != RE::Setting::SETTING_TYPE::kFloat ||
                settingG->GetType() != RE::Setting::SETTING_TYPE::kFloat ||
                settingB->GetType() != RE::Setting::SETTING_TYPE::kFloat) {
                return false;
            }

            outR = settingR->GetFloat();
            outG = settingG->GetFloat();
            outB = settingB->GetFloat();
            return true;
        }

        void ApplyPipboyColorToTheme(Settings& settings, float r, float g, float b)
        {
            const float maxComp = (std::max)({ r, g, b, 0.01f });
            const float normR = r / maxComp;
            const float normG = g / maxComp;
            const float normB = b / maxComp;

            settings.themeAccentR = std::clamp(normR * 0.94f, 0.0f, 1.0f);
            settings.themeAccentG = std::clamp(normG * 0.94f, 0.0f, 1.0f);
            settings.themeAccentB = std::clamp(normB * 0.94f, 0.0f, 1.0f);
            settings.themeAccentA = 1.0f;

            settings.themeWindowR = std::clamp(normR * 0.06f, 0.0f, 1.0f);
            settings.themeWindowG = std::clamp(normG * 0.06f, 0.0f, 1.0f);
            settings.themeWindowB = std::clamp(normB * 0.06f, 0.0f, 1.0f);
            settings.themeWindowA = 0.96f;

            settings.themePanelR = std::clamp(normR * 0.11f, 0.0f, 1.0f);
            settings.themePanelG = std::clamp(normG * 0.11f, 0.0f, 1.0f);
            settings.themePanelB = std::clamp(normB * 0.11f, 0.0f, 1.0f);
            settings.themePanelA = 0.94f;
            ClearThemePreset(settings);
        }

        void ApplyPipboyColorToTheme(Settings& settings)
        {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (!TryReadPipboyColor(r, g, b)) {
                return;
            }

            ApplyPipboyColorToTheme(settings, r, g, b);
        }

        struct PipboyColorCache
        {
            float r{ 0.0f };
            float g{ 0.0f };
            float b{ 0.0f };
            bool valid{ false };
        };

        PipboyColorCache PollPipboyColorCache(bool forceRefresh = false)
        {
            static PipboyColorCache cachedColor{};
            static int pollCounter{ 0 };

            if (forceRefresh || ++pollCounter >= 60) {
                pollCounter = 0;
                cachedColor.valid = TryReadPipboyColor(cachedColor.r, cachedColor.g, cachedColor.b);
            }

            return cachedColor;
        }

        void CenterNextModal()
        {
            if (const auto* viewport = ImGui::GetMainViewport()) {
                ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            }
        }

    }

    void SettingsTab::Draw()
    {
        auto& settings = Config::GetMutable();
        const auto& style = ImGui::GetStyle();
        const auto sectionSpacing = []() {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
        };
        const auto blockSpacing = []() {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
        };
        const auto contentWidth = []() {
            const float leftInset = (std::max)(0.0f, ImGui::GetCursorPosX() - ImGui::GetCursorStartPos().x);
            return (std::max)(ImGui::GetContentRegionAvail().x - leftInset, 120.0f);
        };
        const auto fullWidth = [&contentWidth]() {
            return contentWidth();
        };
        const auto buttonRowWidth = [&contentWidth]() {
            return contentWidth();
        };
        const auto fieldLabel = [](const char* label) {
            ImGui::TextDisabled("%s", label);
        };
        const auto dualButtonWidth = [&style, &buttonRowWidth]() {
            const float available = buttonRowWidth();
            return (std::max)((available - style.ItemSpacing.x) * 0.5f, 160.0f);
        };
        const float popupScale = (std::clamp)(settings.fontSize / 20.0f, 0.75f, 1.5f);
        const auto setConfirmationPopupSizing = [&](const char* primaryLabel, const char* secondaryLabel) {
            const float primaryWidth = ImGui::CalcTextSize(primaryLabel).x + style.FramePadding.x * 2.0f + 36.0f;
            const float secondaryWidth = ImGui::CalcTextSize(secondaryLabel).x + style.FramePadding.x * 2.0f + 36.0f;
            const float popupWidth = (std::max)(420.0f * popupScale, (std::max)(primaryWidth, secondaryWidth) + style.WindowPadding.x * 2.0f);
            ModalUtils::SetNextPopupWindowSizing(
                ImVec2(popupWidth, 210.0f * popupScale),
                ImVec2(popupWidth, 170.0f * popupScale),
                ImVec2(popupWidth * 1.2f, 320.0f * popupScale),
                false);
        };

        if (!ImGui::BeginChild("SettingsScrollRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            ImGui::EndChild();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y + 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x + 2.0f, style.FramePadding.y + 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, style.IndentSpacing + 8.0f);

        const auto& languages = GetAvailableLanguages();
        const auto& fontSizeLabels = GetFontSizeLabels();
        auto currentLanguage = settings.language;
        if (currentLanguage.empty()) {
            currentLanguage = "en";
        }

        bool changed = false;
        bool languageChanged = false;
        const auto pipboyColor = settings.syncPipboyColor ? PollPipboyColorCache() : PipboyColorCache{};
        const std::string resetVisualPopupId = std::string(L("Settings", "sResetVisualSettingsTitle", "Reset Visual Settings")) + "##ResetVisualSettingsPopup";
        const std::string resetAllPopupId = std::string(L("Settings", "sResetAllSettingsTitle", "Reset All Settings")) + "##ResetAllSettingsPopup";
        const std::string allowMainMenuActionsPopupId = std::string(L("Settings", "sAllowMainMenuActionsWarningTitle", "Unsafe Main Menu Actions")) + "##AllowMainMenuActionsPopup";

        if (CaptureToggleKey(settings)) {
            changed = true;
        }

        if (BeginSection("SettingsGeneralSection", L("Settings", "sGeneralSection", "General"))) {
            sectionSpacing();
            ImGui::TextDisabled("%s", L("Settings", "sToggleKey", "Toggle Key"));
            ImGui::TextUnformatted(GetToggleKeyText(settings.toggleKey).c_str());
            blockSpacing();

            const float keyButtonWidth = dualButtonWidth();
            if (ImGui::Button(waitingForToggleKey ? L("Settings", "sPressAnyKey", "Press any key...") : L("Settings", "sCaptureKey", "Capture Key"), ImVec2(keyButtonWidth, 0.0f))) {
                waitingForToggleKey = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(L("Settings", "sResetKeyDefault", "Reset Key"), ImVec2(keyButtonWidth, 0.0f))) {
                settings.toggleKey = 0x2D;
                waitingForToggleKey = false;
                changed = true;
            }

            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsGameplaySection", L("Settings", "sGameplaySection", "Gameplay"))) {
            sectionSpacing();
            changed = ImGui::Checkbox(L("Settings", "sPauseGameWhenMenuOpen", "Pause Game When Menu Open"), &settings.pauseGameWhenMenuOpen) || changed;
            changed = ImGui::Checkbox(L("Settings", "sHidePlayerHUDWhenMenuOpen", "Hide Player HUD When Menu Open"), &settings.hidePlayerHUDWhenMenuOpen) || changed;
            changed = ImGui::Checkbox(L("Settings", "sGodModeWhenMenuOpen", "Enable God Mode While Menu Open"), &settings.godModeWhenMenuOpen) || changed;
            changed = ImGui::Checkbox(L("Settings", "sComponentSubstitution", "Auto-Substitute Component Items"), &settings.componentSubstitution) || changed;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", L("Settings", "sComponentSubstitutionTooltip", "When enabled, giving or spawning a component (CMPO) automatically substitutes the usable scrap item (MISC) so it works for crafting. Disable if you want to give the raw component form."));
            }
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsInterfaceSection", L("Settings", "sInterfaceSection", "Interface"))) {
            sectionSpacing();
            changed = ImGui::Checkbox(L("Settings", "sShowOnStartup", "Show On Startup"), &settings.showOnStartup) || changed;
            changed = ImGui::Checkbox(L("Settings", "sRememberWindowPos", "Remember Window Position"), &settings.rememberWindowPos) || changed;
            changed = ImGui::Checkbox(L("Settings", "sShowFPSStatus", "Show FPS In Status Bar"), &settings.showFPSInStatus) || changed;
            changed = ImGui::Checkbox(L("Settings", "sShowPlayerStats", "Show Player Stats In Status Bar"), &settings.showPlayerStatsInStatus) || changed;
            changed = ImGui::Checkbox(L("Settings", "sAutoFocusSearch", "Auto-Focus Search Bars"), &settings.autoFocusSearchBars) || changed;
            changed = ImGui::Checkbox(L("Settings", "sAdvancedPluginDetails", "Advanced Plugin Browser Details"), &settings.pluginAdvancedDetailsView) || changed;
            sectionSpacing();
            fieldLabel(L("Settings", "sRecentRecordsLimit", "Max Recent Records Displayed"));
            ImGui::SetNextItemWidth(fullWidth());
            changed = ImGui::SliderInt("##RecentRecordsLimit", &settings.recentRecordsLimit, 5, 100) || changed;
            settings.recentRecordsLimit = (std::clamp)(settings.recentRecordsLimit, 5, 100);
            sectionSpacing();

            {
                const auto* startupOption = FindStartupTabOption(settings.startupTab);
                fieldLabel(L("Settings", "sStartupTab", "Startup Tab"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::BeginCombo("##StartupTab", L(startupOption->section, startupOption->key, startupOption->fallback))) {
                    for (const auto& option : kStartupTabOptions) {
                        const bool selected = settings.startupTab == option.value;
                        if (ImGui::Selectable(L(option.section, option.key, option.fallback), selected)) {
                            settings.startupTab = option.value;
                            changed = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            blockSpacing();

            {
                int currentIdx = FontManager::GetCurrentSizeIndex();
                fieldLabel(L("Settings", "sFontSize", "Font Size"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::BeginCombo("##FontSize", fontSizeLabels[currentIdx])) {
                    for (int i = 0; i < FontManager::kPresetCount; ++i) {
                        const bool selected = (i == currentIdx);
                        if (ImGui::Selectable(fontSizeLabels[i], selected)) {
                            FontManager::SetCurrentSizeIndex(i);
                            settings.fontSize = FontManager::kPresetSizes[i];
                            changed = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            blockSpacing();
            fieldLabel(L("Settings", "sWindowOpacity", "Window Opacity"));
            ImGui::SetNextItemWidth(fullWidth());
            changed = ImGui::SliderFloat("##WindowOpacity", &settings.windowAlpha, 0.50f, 1.0f, "%.2f") || changed;
            blockSpacing();
            if (ImGui::Button(L("Settings", "sResetVisualSettings", "Reset Visual Settings"), ImVec2(buttonRowWidth(), 0.0f))) {
                ImGui::OpenPopup(resetVisualPopupId.c_str());
            }

            CenterNextModal();
            const char* resetVisualConfirmLabel = L("Settings", "sResetVisualSettingsConfirm", "Reset Visuals");
            const char* cancelLabel = L("General", "sCancel", "Cancel");
            setConfirmationPopupSizing(resetVisualConfirmLabel, cancelLabel);
            if (ImGui::BeginPopupModal(resetVisualPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped("%s", L("Settings", "sResetVisualSettingsWarning", "This will reset font size, window opacity, and theme colors to their defaults."));
                ImGui::Spacing();

                const float popupButtonWidth = ImGui::GetContentRegionAvail().x;
                if (ImGui::Button(resetVisualConfirmLabel, ImVec2(popupButtonWidth, 0.0f))) {
                    ResetVisualSettings(settings);
                    FontManager::SetCurrentSizeIndex(FontManager::FindClosestSizeIndex(settings.fontSize));
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::Button(cancelLabel, ImVec2(popupButtonWidth, 0.0f))) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsThemeSection", L("Settings", "sThemeSection", "Theme"))) {
            sectionSpacing();
            const auto& themes = ThemeManager::GetAvailableThemes();
            const auto currentThemeLabel = GetColorPresetLabel(settings);
            fieldLabel(L("Settings", "sColorPreset", "Color Preset"));
            ImGui::SetNextItemWidth(fullWidth());
            if (ImGui::BeginCombo("##ColorPreset", currentThemeLabel.c_str())) {
                for (std::size_t i = 0; i < themes.size(); ++i) {
                    const auto& theme = themes[i];
                    const auto themeLabel = GetThemeLabel(theme);
                    const bool selected = settings.themePresetId == theme.id && ThemeManager::FindMatchingTheme(settings) == &theme;
                    ImVec4 previewColor(theme.accentR, theme.accentG, theme.accentB, theme.accentA);
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::ColorButton("##PresetColor", previewColor, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(14, 14));
                    ImGui::SameLine();
                    if (ImGui::Selectable(themeLabel.c_str(), selected)) {
                        ThemeManager::ApplyTheme(settings, theme);
                        settings.syncPipboyColor = false;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            blockSpacing();
            if (ImGui::Checkbox(L("Settings", "sSyncPipboyColor", "Sync With Pip-Boy Color"), &settings.syncPipboyColor)) {
                if (settings.syncPipboyColor) {
                    ClearThemePreset(settings);
                }
                if (settings.syncPipboyColor) {
                    const auto refreshedPipboyColor = PollPipboyColorCache(true);
                    if (refreshedPipboyColor.valid) {
                        ApplyPipboyColorToTheme(settings, refreshedPipboyColor.r, refreshedPipboyColor.g, refreshedPipboyColor.b);
                    }
                }
                changed = true;
            }
            if (settings.syncPipboyColor) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", L("Settings", "sAuto", ""));
            }

            if (settings.syncPipboyColor) {
                if (pipboyColor.valid) {
                    ApplyPipboyColorToTheme(settings, pipboyColor.r, pipboyColor.g, pipboyColor.b);
                }
            }

            if (!settings.syncPipboyColor) {
                sectionSpacing();
                float accentColor[4]{ settings.themeAccentR, settings.themeAccentG, settings.themeAccentB, settings.themeAccentA };
                fieldLabel(L("Settings", "sThemeAccent", "Theme Accent"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::ColorEdit4("##ThemeAccent", accentColor, ImGuiColorEditFlags_NoInputs)) {
                    settings.themeAccentR = accentColor[0];
                    settings.themeAccentG = accentColor[1];
                    settings.themeAccentB = accentColor[2];
                    settings.themeAccentA = accentColor[3];
                    ClearThemePreset(settings);
                    changed = true;
                }

                float windowColor[4]{ settings.themeWindowR, settings.themeWindowG, settings.themeWindowB, settings.themeWindowA };
                fieldLabel(L("Settings", "sThemeWindow", "Theme Window"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::ColorEdit4("##ThemeWindow", windowColor, ImGuiColorEditFlags_NoInputs)) {
                    settings.themeWindowR = windowColor[0];
                    settings.themeWindowG = windowColor[1];
                    settings.themeWindowB = windowColor[2];
                    settings.themeWindowA = windowColor[3];
                    ClearThemePreset(settings);
                    changed = true;
                }

                float panelColor[4]{ settings.themePanelR, settings.themePanelG, settings.themePanelB, settings.themePanelA };
                fieldLabel(L("Settings", "sThemePanel", "Theme Panel"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::ColorEdit4("##ThemePanel", panelColor, ImGuiColorEditFlags_NoInputs)) {
                    settings.themePanelR = panelColor[0];
                    settings.themePanelG = panelColor[1];
                    settings.themePanelB = panelColor[2];
                    settings.themePanelA = panelColor[3];
                    ClearThemePreset(settings);
                    changed = true;
                }
            } else {
                if (pipboyColor.valid) {
                    ImVec4 pipColor(pipboyColor.r, pipboyColor.g, pipboyColor.b, 1.0f);
                    ImGui::ColorButton("##PipboyPreview", pipColor, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s: R=%.2f G=%.2f B=%.2f", L("Settings", "sPipboyColor", ""), pipboyColor.r, pipboyColor.g, pipboyColor.b);
                }
            }

            sectionSpacing();
            if (ImGui::Button(L("Settings", "sResetTheme", "Reset Theme"), ImVec2(buttonRowWidth(), 0.0f))) {
                ThemeManager::ApplyTheme(settings, ThemeManager::GetDefaultTheme());
                settings.syncPipboyColor = false;
                changed = true;
            }
            blockSpacing();
            if (ImGui::Button(L("Settings", "sRefreshThemes", "Refresh Themes"), ImVec2(buttonRowWidth(), 0.0f))) {
                const auto selectedPresetId = settings.themePresetId;
                ThemeManager::ReloadAvailableThemes();
                if (!selectedPresetId.empty()) {
                    if (const auto* selectedTheme = ThemeManager::FindThemeById(selectedPresetId)) {
                        ThemeManager::ApplyTheme(settings, *selectedTheme);
                        changed = true;
                    }
                }
            }
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsLocalizationSection", L("Settings", "sLocalizationSection", "Localization"))) {
            std::string currentLanguageLabel = currentLanguage;
            for (const auto& language : languages) {
                if (language.code == currentLanguage) {
                    currentLanguageLabel = BuildLanguageLabel(language);
                    break;
                }
            }

            sectionSpacing();
            fieldLabel(L("Settings", "sLanguage", "Language"));
            ImGui::SetNextItemWidth(fullWidth());
            if (ImGui::BeginCombo("##Language", currentLanguageLabel.c_str())) {
                for (const auto& language : languages) {
                    const std::string label = BuildLanguageLabel(language);
                    const bool selected = language.code == currentLanguage;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        settings.language = language.code;
                        currentLanguage = language.code;
                        languageChanged = true;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsControllerSection", L("Settings", "sControllerSection", "Controller"))) {
            sectionSpacing();
            changed = ImGui::Checkbox(L("Settings", "sEnableGamepadNav", "Enable Gamepad Navigation"), &settings.enableGamepadNav) || changed;
            sectionSpacing();
            ImGui::TextDisabled("%s: %s", L("Settings", "sGamepadStatus", "Gamepad"), GamepadInput::IsGamepadConnected() ? L("Settings", "sConnected", "Connected") : L("Settings", "sDisconnected", "Disconnected"));
            ImGui::TextDisabled("%s: %s", L("Settings", "sControllerToggle", "Toggle"), L("Settings", "sToggleCombo", "Back + Start"));
            ImGui::TextDisabled("%s: %s / %s", L("Settings", "sNavigation", "Navigation"), L("Settings", "sDPad", "D-Pad"), L("Settings", "sLeftStick", "Left Stick"));
            ImGui::TextDisabled("%s: %s  |  %s: %s", L("Settings", "sConfirm", "Confirm"), L("Settings", "sButtonA", "A"), L("Settings", "sGoBack", "Back"), L("Settings", "sButtonB", "B"));
            ImGui::TextDisabled("%s: %s", L("Settings", "sTabSwitch", "Tab Switch"), L("Settings", "sShoulderButtons", "Shoulder Buttons"));
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsLoggingSection", L("Settings", "sLoggingSection", "Logging"))) {
            sectionSpacing();
            changed = ImGui::Checkbox(L("Settings", "sShowLogsTab", "Show Logs Tab"), &settings.showLogsTab) || changed;
            if (ImGui::Checkbox(L("Settings", "sVerboseLogging", "Verbose Logging"), &settings.verboseLogging)) {
                Logger::SetVerboseEnabled(settings.verboseLogging);
                changed = true;
            }
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsDebugSection", L("Settings", "sDebugSection", "Debug"))) {
            sectionSpacing();
            changed = ImGui::Checkbox(L("Settings", "sShowMenuResolutionStatus", "Show Menu Resolution In Status Bar"), &settings.showMenuResolutionInStatus) || changed;
            if (ImGui::Checkbox(L("Settings", "sAllowMainMenuActions", "Allow Gameplay Actions In Main Menu (Unsafe)"), &settings.allowGameplayActionsInMainMenu)) {
                if (settings.allowGameplayActionsInMainMenu) {
                    settings.allowGameplayActionsInMainMenu = false;
                    ImGui::OpenPopup(allowMainMenuActionsPopupId.c_str());
                } else {
                    changed = true;
                }
            }

            CenterNextModal();
            const char* unsafeConfirmLabel = L("Settings", "sAllowMainMenuActionsConfirm", "Enable Unsafe Actions");
            const char* unsafeCancelLabel = L("General", "sCancel", "Cancel");
            setConfirmationPopupSizing(unsafeConfirmLabel, unsafeCancelLabel);
            if (ImGui::BeginPopupModal(allowMainMenuActionsPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped("%s", L("Settings", "sAllowMainMenuActionsWarning", "Enabling gameplay actions while the main menu is open will likely cause unexpected behavior or crashes. Only use this for debugging."));
                ImGui::Spacing();

                const float popupButtonWidth = ImGui::GetContentRegionAvail().x;
                if (ImGui::Button(unsafeConfirmLabel, ImVec2(popupButtonWidth, 0.0f))) {
                    settings.allowGameplayActionsInMainMenu = true;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::Button(unsafeCancelLabel, ImVec2(popupButtonWidth, 0.0f))) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            sectionSpacing();
            ImGui::TreePop();
        }

        sectionSpacing();
        if (ImGui::Button(L("Settings", "sResetAllSettings", "Reset All Settings"), ImVec2(buttonRowWidth(), 0.0f))) {
            ImGui::OpenPopup(resetAllPopupId.c_str());
        }

        CenterNextModal();
        const char* resetAllConfirmLabel = L("Settings", "sResetAllSettingsConfirm", "Reset to Defaults");
        const char* resetAllCancelLabel = L("General", "sCancel", "Cancel");
        setConfirmationPopupSizing(resetAllConfirmLabel, resetAllCancelLabel);
        if (ImGui::BeginPopupModal(resetAllPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", L("Settings", "sResetAllSettingsWarning", "This will reset all saved settings, window state, filters, favorites, theme, and language to their defaults."));
            ImGui::Spacing();

            const float popupButtonWidth = ImGui::GetContentRegionAvail().x;
            if (ImGui::Button(resetAllConfirmLabel, ImVec2(popupButtonWidth, 0.0f))) {
                const bool resetLanguage = settings.language != Settings{}.language;
                Config::ResetToDefaults();
                MainWindow::ResetStateFromConfig();
                waitingForToggleKey = false;
                currentLanguage = settings.language;
                FontManager::SetCurrentSizeIndex(FontManager::FindClosestSizeIndex(settings.fontSize));
                Logger::SetVerboseEnabled(settings.verboseLogging);
                changed = true;
                languageChanged = languageChanged || resetLanguage;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Button(resetAllCancelLabel, ImVec2(popupButtonWidth, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Spacing();
        SharedUtils::DrawSectionLabel(L("Settings", "sAboutSection", "About"));
        ImGui::Spacing();
        ImGui::TextDisabled("%s: %s", L("Settings", "sGameVersion", "Game Version"), GetGameVersionText().c_str());
        ImGui::TextDisabled("%s: %s", L("Settings", "sModVersion", "Mod Version"), GetModVersionText().c_str());
        ImGui::Spacing();
        sectionSpacing();

        const float aboutButtonWidth = dualButtonWidth();
        if (ImGui::Button(L("Settings", "sOpenNexusMods", "Open Nexus Mods Page"), ImVec2(aboutButtonWidth, 0.0f))) {
            ShellExecuteA(nullptr, "open", kNexusModsUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button(L("Settings", "sOpenGitHub", "GitHub"), ImVec2(aboutButtonWidth, 0.0f))) {
            ShellExecuteA(nullptr, "open", kGitHubUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (ImGui::Button(L("Settings", "sOpenBugReport", "Report a Bug"), ImVec2(aboutButtonWidth, 0.0f))) {
            ShellExecuteA(nullptr, "open", kNexusBugReportUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        if (ImGui::Button(L("Settings", "sOpenBuyMeACoffee", "Buy Me A Coffee"), ImVec2(aboutButtonWidth, 0.0f))) {
            ShellExecuteA(nullptr, "open", kBuyMeACoffeeUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (ImGui::Button(L("Settings", "sShowHelpOverlay", "Show Help Overlay"), ImVec2(buttonRowWidth(), 0.0f))) {
            MainWindowPopups::OpenHelpOverlay();
        }
        sectionSpacing();

        ImGui::PopStyleVar(3);

        AutoPersist(changed);
        if (languageChanged) {
            Language::Load(settings.language);
        }
        if (languageChanged) {
            FontManager::RequestLanguageRebuild(settings.language);
        }

        ImGui::EndChild();
    }
}
