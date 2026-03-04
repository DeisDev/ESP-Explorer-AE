#include "GUI/Tabs/SettingsTab.h"

#include "Config/Config.h"
#include "Input/GamepadInput.h"
#include "Logging/Logger.h"
#include "Localization/FontManager.h"
#include "Localization/Language.h"

#include <imgui.h>

#include <RE/S/Setting.h>
#include <REL/Version.h>

namespace ESPExplorerAE
{
    namespace
    {
        bool waitingForToggleKey{ false };
        constexpr auto kNexusModsUrl = "https://www.nexusmods.com/fallout4/mods/00000";

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
                    if (vk == 0) {
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

            Config::Save();
            return true;
        }

        std::string GetGameVersionText()
        {
            static const std::string cachedVersion = []() {
                if (const auto version = REL::GetFileVersion(std::string_view("Fallout4.exe")); version.has_value()) {
                    return version->string();
                }
                return std::string(L("General", "sUnknown", "Unknown"));
            }();

            return cachedVersion;
        }

        struct ColorPreset
        {
            const char* key;
            float accentR, accentG, accentB, accentA;
            float windowR, windowG, windowB, windowA;
            float panelR, panelG, panelB, panelA;
        };

        constexpr ColorPreset kColorPresets[] = {
            { "sPresetDefaultGreen", 0.27f, 0.94f, 0.38f, 1.0f, 0.03f, 0.08f, 0.05f, 0.96f, 0.06f, 0.14f, 0.09f, 0.94f },
            { "sPresetPipBoyAmber", 1.00f, 0.80f, 0.24f, 1.0f, 0.08f, 0.06f, 0.02f, 0.96f, 0.14f, 0.10f, 0.04f, 0.94f },
            { "sPresetPipBoyBlue", 0.38f, 0.72f, 1.00f, 1.0f, 0.02f, 0.04f, 0.10f, 0.96f, 0.05f, 0.08f, 0.16f, 0.94f },
            { "sPresetPipBoyWhite", 0.92f, 0.94f, 0.96f, 1.0f, 0.06f, 0.06f, 0.07f, 0.96f, 0.10f, 0.10f, 0.12f, 0.94f },
            { "sPresetNukaRed", 1.00f, 0.30f, 0.28f, 1.0f, 0.10f, 0.03f, 0.03f, 0.96f, 0.16f, 0.05f, 0.05f, 0.94f },
            { "sPresetInstitute", 0.42f, 0.88f, 0.98f, 1.0f, 0.02f, 0.05f, 0.08f, 0.96f, 0.04f, 0.09f, 0.14f, 0.94f },
            { "sPresetBrotherhood", 0.85f, 0.65f, 0.30f, 1.0f, 0.07f, 0.05f, 0.02f, 0.96f, 0.12f, 0.09f, 0.04f, 0.94f },
            { "sPresetRailroad", 0.75f, 0.45f, 0.80f, 1.0f, 0.06f, 0.03f, 0.07f, 0.96f, 0.10f, 0.05f, 0.12f, 0.94f },
            { "sPresetMinutemen", 0.50f, 0.75f, 0.95f, 1.0f, 0.03f, 0.05f, 0.08f, 0.96f, 0.06f, 0.09f, 0.14f, 0.94f },
            { "sPresetVaultTec", 0.25f, 0.55f, 0.95f, 1.0f, 0.02f, 0.03f, 0.09f, 0.96f, 0.04f, 0.06f, 0.15f, 0.94f },
            { "sPresetStealth", 0.55f, 0.55f, 0.58f, 1.0f, 0.04f, 0.04f, 0.05f, 0.96f, 0.07f, 0.07f, 0.08f, 0.94f },
            { "sPresetNeonPink", 1.00f, 0.40f, 0.70f, 1.0f, 0.08f, 0.03f, 0.05f, 0.96f, 0.14f, 0.05f, 0.09f, 0.94f },
        };
        constexpr int kColorPresetCount = static_cast<int>(std::size(kColorPresets));

        void ApplyColorPreset(Settings& settings, const ColorPreset& preset)
        {
            settings.themeAccentR = preset.accentR;
            settings.themeAccentG = preset.accentG;
            settings.themeAccentB = preset.accentB;
            settings.themeAccentA = preset.accentA;
            settings.themeWindowR = preset.windowR;
            settings.themeWindowG = preset.windowG;
            settings.themeWindowB = preset.windowB;
            settings.themeWindowA = preset.windowA;
            settings.themePanelR = preset.panelR;
            settings.themePanelG = preset.panelG;
            settings.themePanelB = preset.panelB;
            settings.themePanelA = preset.panelA;
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

        void ApplyPipboyColorToTheme(Settings& settings)
        {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            if (!TryReadPipboyColor(r, g, b)) {
                return;
            }

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
        }

        std::string GetModVersionText()
        {
            const auto version = F4SE::GetPluginVersion();
            return version.string();
        }
    }

    void SettingsTab::Draw()
    {
        auto& settings = Config::GetMutable();

        if (!ImGui::BeginChild("SettingsScrollRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            ImGui::EndChild();
            return;
        }

        const auto languages = Language::ListAvailableLanguages();
        auto currentLanguage = settings.language;
        if (currentLanguage.empty()) {
            currentLanguage = "en";
        }

        bool changed = false;
        bool languageChanged = false;

        if (CaptureToggleKey(settings)) {
            changed = true;
        }

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sGeneralSection", "General") + std::string("##SettingsGeneralSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            ImGui::Text("%s: %s", L("Settings", "sToggleKey", "Toggle Key"), KeyNameFromVK(settings.toggleKey).c_str());
            ImGui::SameLine();
            if (ImGui::Button(waitingForToggleKey ? L("Settings", "sPressAnyKey", "Press any key...") : L("Settings", "sCaptureKey", "Capture Key"))) {
                waitingForToggleKey = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(L("Settings", "sResetKeyDefault", "Reset Key"))) {
                settings.toggleKey = 0x2D;
                waitingForToggleKey = false;
                changed = true;
            }

            changed = ImGui::Checkbox(L("Settings", "sShowOnStartup", "Show On Startup"), &settings.showOnStartup) || changed;
            changed = ImGui::Checkbox(L("Settings", "sNoPauseOnFocusLoss", "Do Not Pause On Focus Loss"), &settings.noPauseOnFocusLoss) || changed;
            changed = ImGui::Checkbox(L("Settings", "sPauseGameWhenMenuOpen", "Pause Game When Menu Open"), &settings.pauseGameWhenMenuOpen) || changed;
            if (ImGui::Checkbox(L("Settings", "sVerboseLogging", "Verbose Logging"), &settings.verboseLogging)) {
                Logger::SetVerboseEnabled(settings.verboseLogging);
                changed = true;
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sInterfaceSection", "Interface") + std::string("##SettingsInterfaceSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            changed = ImGui::Checkbox(L("Settings", "sRememberWindowPos", "Remember Window Position"), &settings.rememberWindowPos) || changed;
            changed = ImGui::Checkbox(L("Settings", "sShowFPSStatus", "Show FPS In Status Bar"), &settings.showFPSInStatus) || changed;
            changed = ImGui::Checkbox(L("Settings", "sShowPlayerStats", "Show Player Stats In Status Bar"), &settings.showPlayerStatsInStatus) || changed;
            changed = ImGui::Checkbox(L("Settings", "sAutoFocusSearch", "Auto-Focus Search Bars"), &settings.autoFocusSearchBars) || changed;

            {
                int currentIdx = FontManager::GetCurrentSizeIndex();
                char previewBuf[16]{};
                std::snprintf(previewBuf, sizeof(previewBuf), "%.0f px", FontManager::kPresetSizes[currentIdx]);
                if (ImGui::BeginCombo(L("Settings", "sFontSize", "Font Size"), previewBuf)) {
                    for (int i = 0; i < FontManager::kPresetCount; ++i) {
                        char label[16]{};
                        std::snprintf(label, sizeof(label), "%.0f px", FontManager::kPresetSizes[i]);
                        const bool selected = (i == currentIdx);
                        if (ImGui::Selectable(label, selected)) {
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
            changed = ImGui::SliderFloat(L("Settings", "sWindowOpacity", "Window Opacity"), &settings.windowAlpha, 0.50f, 1.0f, "%.2f") || changed;
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sThemeSection", "Theme") + std::string("##SettingsThemeSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            if (ImGui::BeginCombo(L("Settings", "sColorPreset", ""), L("Settings", "sSelectPreset", ""))) {
                for (int i = 0; i < kColorPresetCount; ++i) {
                    const auto& preset = kColorPresets[i];
                    ImVec4 previewColor(preset.accentR, preset.accentG, preset.accentB, preset.accentA);
                    ImGui::ColorButton(("##PresetColor" + std::to_string(i)).c_str(), previewColor, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(14, 14));
                    ImGui::SameLine();
                    if (ImGui::Selectable(L("Settings", preset.key, ""), false)) {
                        ApplyColorPreset(settings, preset);
                        settings.syncPipboyColor = false;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Checkbox(L("Settings", "sSyncPipboyColor", "Sync With Pip-Boy Color"), &settings.syncPipboyColor)) {
                if (settings.syncPipboyColor) {
                    ApplyPipboyColorToTheme(settings);
                }
                changed = true;
            }
            if (settings.syncPipboyColor) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", L("Settings", "sAuto", ""));
            }

            if (settings.syncPipboyColor) {
                static int pipboyPollCounter{ 0 };
                if (++pipboyPollCounter >= 60) {
                    pipboyPollCounter = 0;
                    ApplyPipboyColorToTheme(settings);
                }
            }

            if (!settings.syncPipboyColor) {
                float accentColor[4]{ settings.themeAccentR, settings.themeAccentG, settings.themeAccentB, settings.themeAccentA };
                if (ImGui::ColorEdit4(L("Settings", "sThemeAccent", "Theme Accent"), accentColor, ImGuiColorEditFlags_NoInputs)) {
                    settings.themeAccentR = accentColor[0];
                    settings.themeAccentG = accentColor[1];
                    settings.themeAccentB = accentColor[2];
                    settings.themeAccentA = accentColor[3];
                    changed = true;
                }

                float windowColor[4]{ settings.themeWindowR, settings.themeWindowG, settings.themeWindowB, settings.themeWindowA };
                if (ImGui::ColorEdit4(L("Settings", "sThemeWindow", "Theme Window"), windowColor, ImGuiColorEditFlags_NoInputs)) {
                    settings.themeWindowR = windowColor[0];
                    settings.themeWindowG = windowColor[1];
                    settings.themeWindowB = windowColor[2];
                    settings.themeWindowA = windowColor[3];
                    changed = true;
                }

                float panelColor[4]{ settings.themePanelR, settings.themePanelG, settings.themePanelB, settings.themePanelA };
                if (ImGui::ColorEdit4(L("Settings", "sThemePanel", "Theme Panel"), panelColor, ImGuiColorEditFlags_NoInputs)) {
                    settings.themePanelR = panelColor[0];
                    settings.themePanelG = panelColor[1];
                    settings.themePanelB = panelColor[2];
                    settings.themePanelA = panelColor[3];
                    changed = true;
                }
            } else {
                float pipR = 0, pipG = 0, pipB = 0;
                if (TryReadPipboyColor(pipR, pipG, pipB)) {
                    ImVec4 pipColor(pipR, pipG, pipB, 1.0f);
                    ImGui::ColorButton("##PipboyPreview", pipColor, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s: R=%.2f G=%.2f B=%.2f", L("Settings", "sPipboyColor", ""), pipR, pipG, pipB);
                }
            }

            if (ImGui::Button(L("Settings", "sResetTheme", "Reset Theme"))) {
                ApplyColorPreset(settings, kColorPresets[0]);
                settings.syncPipboyColor = false;
                changed = true;
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sLocalizationSection", "Localization") + std::string("##SettingsLocalizationSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            if (ImGui::BeginCombo(L("Settings", "sLanguage", "Language"), currentLanguage.c_str())) {
                for (const auto& code : languages) {
                    const bool selected = code == currentLanguage;
                    if (ImGui::Selectable(code.c_str(), selected)) {
                        settings.language = code;
                        currentLanguage = code;
                        languageChanged = true;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sControllerSection", "Controller") + std::string("##SettingsControllerSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            changed = ImGui::Checkbox(L("Settings", "sEnableGamepadNav", ""), &settings.enableGamepadNav) || changed;
            ImGui::Spacing();
            ImGui::TextDisabled("%s: %s", L("Settings", "sGamepadStatus", ""), GamepadInput::IsGamepadConnected() ? L("Settings", "sConnected", "") : L("Settings", "sDisconnected", ""));
            ImGui::TextDisabled("%s: %s", L("Settings", "sControllerToggle", ""), L("Settings", "sToggleCombo", ""));
            ImGui::TextDisabled("%s: %s / %s", L("Settings", "sNavigation", ""), L("Settings", "sDPad", ""), L("Settings", "sLeftStick", ""));
            ImGui::TextDisabled("%s: %s  |  %s: %s", L("Settings", "sConfirm", ""), L("Settings", "sButtonA", ""), L("Settings", "sGoBack", ""), L("Settings", "sButtonB", ""));
            ImGui::TextDisabled("%s: %s", L("Settings", "sTabSwitch", ""), L("Settings", "sShoulderButtons", ""));
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sDebugSection", "Debug") + std::string("##SettingsDebugSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            changed = ImGui::Checkbox(L("Settings", "sShowMenuResolutionStatus", "Show Menu Resolution In Status Bar"), &settings.showMenuResolutionInStatus) || changed;
            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::SeparatorText(L("Settings", "sAboutSection", "About"));
        ImGui::Spacing();
        ImGui::TextDisabled("%s: %s", L("Settings", "sGameVersion", "Game Version"), GetGameVersionText().c_str());
        ImGui::TextDisabled("%s: %s", L("Settings", "sModVersion", "Mod Version"), GetModVersionText().c_str());
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextDisabled("%s", kNexusModsUrl);
        ImGui::Spacing();
        if (ImGui::Button(L("Settings", "sOpenNexusMods", "Open Nexus Mods Page"))) {
            ShellExecuteA(nullptr, "open", kNexusModsUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }

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
