#include "GUI/Tabs/SettingsTab.h"

#include "Config/Config.h"
#include "Logging/Logger.h"
#include "Localization/FontManager.h"
#include "Localization/Language.h"

#include <imgui.h>

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

        std::string GetModVersionText()
        {
            const auto version = F4SE::GetPluginVersion();
            return version.string();
        }
    }

    void SettingsTab::Draw()
    {
        auto& settings = Config::GetMutable();

        const auto languages = Language::ListAvailableLanguages();
        auto currentLanguage = settings.language;
        if (currentLanguage.empty()) {
            currentLanguage = "en";
        }

        bool changed = false;
        bool languageChanged = false;
        bool fontChanged = false;

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
            if (ImGui::Checkbox(L("Settings", "sVerboseLogging", "Verbose Logging"), &settings.verboseLogging)) {
                Logger::SetVerboseEnabled(settings.verboseLogging);
                changed = true;
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sInterfaceSection", "Interface") + std::string("##SettingsInterfaceSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
            changed = ImGui::Checkbox(L("Settings", "sRememberWindowPos", "Remember Window Position"), &settings.rememberWindowPos) || changed;
            changed = ImGui::Checkbox(L("Settings", "sShowFPSStatus", "Show FPS In Status Bar"), &settings.showFPSInStatus) || changed;
            if (ImGui::SliderFloat(L("Settings", "sFontSize", "Font Size"), &settings.fontSize, 12.0f, 30.0f, "%.1f")) {
                fontChanged = true;
                changed = true;
            }
            changed = ImGui::SliderFloat(L("Settings", "sWindowOpacity", "Window Opacity"), &settings.windowAlpha, 0.50f, 1.0f, "%.2f") || changed;
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx(std::string(L("Settings", "sThemeSection", "Theme") + std::string("##SettingsThemeSection")).c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding)) {
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

            if (ImGui::Button(L("Settings", "sResetTheme", "Reset Theme"))) {
                settings.themeAccentR = 0.27f;
                settings.themeAccentG = 0.94f;
                settings.themeAccentB = 0.38f;
                settings.themeAccentA = 1.0f;
                settings.themeWindowR = 0.03f;
                settings.themeWindowG = 0.08f;
                settings.themeWindowB = 0.05f;
                settings.themeWindowA = 0.96f;
                settings.themePanelR = 0.06f;
                settings.themePanelG = 0.14f;
                settings.themePanelB = 0.09f;
                settings.themePanelA = 0.94f;
                changed = true;
            }
            ImGui::TreePop();
        }

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

            if (ImGui::Button(L("Settings", "sReloadLanguage", "Reload Language"))) {
                Language::Load(settings.language);
            }
            ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::SeparatorText(L("Settings", "sAboutSection", "About"));
        ImGui::TextDisabled("%s: %s", L("Settings", "sGameVersion", "Game Version"), GetGameVersionText().c_str());
        ImGui::TextDisabled("%s: %s", L("Settings", "sModVersion", "Mod Version"), GetModVersionText().c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("%s", kNexusModsUrl);
        if (ImGui::Button(L("Settings", "sOpenNexusMods", "Open Nexus Mods Page"))) {
            ShellExecuteA(nullptr, "open", kNexusModsUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }

        AutoPersist(changed);
        if (languageChanged) {
            Language::Load(settings.language);
        }
        if (fontChanged || languageChanged) {
            FontManager::Build(settings.fontSize, settings.language);
        }
    }
}
