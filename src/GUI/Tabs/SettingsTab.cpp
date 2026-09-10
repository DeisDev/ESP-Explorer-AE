#include "GUI/Tabs/SettingsTab.h"

#include "Core/SettingsValidation.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Widgets/SharedUtils.h"
#include <imgui.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace ESPExplorerAE
{
    namespace
    {
        class SettingsView
        {
            SettingsTabState& state;
            const SettingsTabView& view;
            SettingsTabRequests& requests;
        public:
            SettingsView(SettingsTabState& owned, const SettingsTabView& input, SettingsTabRequests& output) : state(owned), view(input), requests(output) {}
            const char* L(std::string_view section, std::string_view key, const char* fallback) { return view.localize ? view.localize(section, key, fallback) : fallback; }

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
                if (!state.waitingForToggleKey) {
                    return false;
                }

                for (int keyIndex = ImGuiKey_NamedKey_BEGIN; keyIndex < ImGuiKey_NamedKey_END; ++keyIndex) {
                    const auto key = static_cast<ImGuiKey>(keyIndex);
                    if (ImGui::IsKeyPressed(key, false)) {
                        if (key == ImGuiKey_Escape) {
                            state.waitingForToggleKey = false;
                            return false;
                        }

                        const auto vk = ImGuiKeyToVK(key);
                        if (vk == 0 || vk == VK_TAB) {
                            continue;
                        }

                        settings.toggleKey = vk;
                        state.waitingForToggleKey = false;
                        return true;
                    }
                }

                return false;
            }

            static constexpr auto kStartupTabLastActive = "__last__";

            struct StartupTabOption
            {
                const char* value;
                const char* section;
                const char* key;
                const char* fallback;
            };

            static constexpr StartupTabOption kStartupTabOptions[] = {
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

            struct MultiCopyFormatOption
            {
                MultiCopyFormat value;
                const char* key;
                const char* fallback;
            };

            static constexpr MultiCopyFormatOption kMultiCopyFormatOptions[] = {
                { MultiCopyFormat::Lines, "sMultiCopyFormatLines", "One Per Line" },
                { MultiCopyFormat::CommaSeparated, "sMultiCopyFormatComma", "Comma Separated" },
                { MultiCopyFormat::Parenthesized, "sMultiCopyFormatParenthesized", "Parenthesized" },
                { MultiCopyFormat::QuotedCommaSeparated, "sMultiCopyFormatQuotedComma", "Quoted Comma Separated" },
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

            const MultiCopyFormatOption* FindMultiCopyFormatOption(MultiCopyFormat value)
            {
                for (const auto& option : kMultiCopyFormatOptions) {
                    if (option.value == value) {
                        return &option;
                    }
                }

                return &kMultiCopyFormatOptions[0];
            }

            std::string BuildLanguageLabel(const LanguageDefinition& language)
            {
                if (language.displayName.empty() || language.displayName == language.code) {
                    return language.code;
                }

                return language.displayName + " (" + language.code + ")";
            }

            bool BeginSection(const char* id, const char* label)
            {
                ImGui::PushID(id);
                const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_OpenOnArrow);
                SharedUtils::DrawCurrentItemChrome(open, ImGui::IsItemHovered(), false, true);
                ImGui::PopID();
                return open;
            }

            std::array<std::string, FontSizes.size()> GetFontSizeLabels()
            {
                std::array<std::string, FontSizes.size()> labels;
                for (std::size_t i = 0; i < labels.size(); ++i) {
                    labels[i] = std::to_string(static_cast<int>(FontSizes[i])) + " " + L("Settings", "sPixelUnit", "px");
                }
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
                    const auto* selectedTheme = FindTheme(*view.resources.themes, settings.themePresetId);
                    const auto* matchingTheme = FindMatchingTheme(*view.resources.themes, settings);
                    if (selectedTheme && selectedTheme == matchingTheme) {
                        return GetThemeLabel(*selectedTheme);
                    }

                    return std::string(L("Settings", "sThemePresetCustom", "Custom"));
                }

                if (const auto* matchingTheme = FindMatchingTheme(*view.resources.themes, settings)) {
                    return GetThemeLabel(*matchingTheme);
                }

                return std::string(L("Settings", "sThemePresetCustom", "Custom"));
            }

            void ClearThemePreset(Settings& settings)
            {
                settings.themePresetId.clear();
            }

            void CenterNextModal()
            {
                if (const auto* viewport = ImGui::GetMainViewport()) {
                    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                }
            }


            void Draw()
            {
                auto settings = view.settings;
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
            const ModalUtils::PopupSizing popupSizing(
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

        const auto& languages = *view.resources.languages;
        const auto& fontSizeLabels = GetFontSizeLabels();
        auto currentLanguage = settings.language;
        if (currentLanguage.empty()) {
            currentLanguage = "en";
        }

        bool changed = false;
        const auto pipboyColor = view.resources.pipboyColor;
        const std::string resetVisualPopupId = std::string(L("Settings", "sResetVisualSettingsTitle", "Reset Visual Settings")) + "##ResetVisualSettingsPopup";
        const std::string resetAllPopupId = std::string(L("Settings", "sResetAllSettingsTitle", "Reset All Settings")) + "##ResetAllSettingsPopup";
        const std::string allowMainMenuActionsPopupId = std::string(L("Settings", "sAllowMainMenuActionsWarningTitle", "Unsafe Main Menu Actions")) + "##AllowMainMenuActionsPopup";

        if (CaptureToggleKey(settings)) {
            changed = true;
        }

        if (BeginSection("SettingsGeneralSection", L("Settings", "sGeneralSection", "General"))) {
            sectionSpacing();
            ImGui::TextDisabled("%s", L("Settings", "sToggleKey", "Toggle Key"));
            ImGui::TextUnformatted(view.resources.toggleKeyName.c_str());
            blockSpacing();

            const float keyButtonWidth = dualButtonWidth();
            if (ImGui::Button(state.waitingForToggleKey ? L("Settings", "sPressAnyKey", "Press any key...") : L("Settings", "sCaptureKey", "Capture Key"), ImVec2(keyButtonWidth, 0.0f))) {
                state.waitingForToggleKey = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(L("Settings", "sResetKeyDefault", "Reset Key"), ImVec2(keyButtonWidth, 0.0f))) {
                settings.toggleKey = 0x2D;
                state.waitingForToggleKey = false;
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
            blockSpacing();
            changed = ImGui::Checkbox(L("Settings", "sIncludeAmmoWithWeapons", "Include Ammo With Weapons By Default"), &settings.includeAmmoWithWeapons) || changed;
            fieldLabel(L("Settings", "sDefaultAmmoQuantity", "Default Extra Ammo Quantity"));
            ImGui::SetNextItemWidth(fullWidth());
            if (ImGui::InputInt("##DefaultAmmoQuantity", &settings.defaultAmmoQuantity, 50, 100)) {
                settings.defaultAmmoQuantity = (std::clamp)(settings.defaultAmmoQuantity, 0, 50000);
                changed = true;
            }
            ImGui::TextWrapped("%s", L("Settings", "sAmmoDefaultsHelp", "Used when opening Add Item for a weapon. Extra ammo is added once per weapon entry, not per copy. You can change it in the popup."));
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
            blockSpacing();

            {
                const auto* multiCopyOption = FindMultiCopyFormatOption(settings.multiCopyFormat);
                fieldLabel(L("Settings", "sMultiCopyFormat", "Multi-Copy Format"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::BeginCombo("##MultiCopyFormat", L("Settings", multiCopyOption->key, multiCopyOption->fallback))) {
                    for (const auto& option : kMultiCopyFormatOptions) {
                        const bool selected = settings.multiCopyFormat == option.value;
                        if (ImGui::Selectable(L("Settings", option.key, option.fallback), selected)) {
                            settings.multiCopyFormat = option.value;
                            changed = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
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
                int currentIdx = ClosestFontSizeIndex(settings.fontSize);
                fieldLabel(L("Settings", "sFontSize", "Font Size"));
                ImGui::SetNextItemWidth(fullWidth());
                if (ImGui::BeginCombo("##FontSize", fontSizeLabels[currentIdx].c_str())) {
                    for (int i = 0; i < static_cast<int>(FontSizes.size()); ++i) {
                        const bool selected = (i == currentIdx);
                        if (ImGui::Selectable(fontSizeLabels[i].c_str(), selected)) {
                            settings.fontSize = FontSizes[i];
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
            const auto& themes = *view.resources.themes;
            const auto currentThemeLabel = GetColorPresetLabel(settings);
            fieldLabel(L("Settings", "sColorPreset", "Color Preset"));
            ImGui::SetNextItemWidth(fullWidth());
            if (ImGui::BeginCombo("##ColorPreset", currentThemeLabel.c_str())) {
                for (std::size_t i = 0; i < themes.size(); ++i) {
                    const auto& theme = themes[i];
                    const auto themeLabel = GetThemeLabel(theme);
                    const bool selected = settings.themePresetId == theme.id && FindMatchingTheme(*view.resources.themes, settings) == &theme;
                    ImVec4 previewColor(theme.accentR, theme.accentG, theme.accentB, theme.accentA);
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::ColorButton("##PresetColor", previewColor, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder, ImVec2(14, 14));
                    ImGui::SameLine();
                    if (ImGui::Selectable(themeLabel.c_str(), selected)) {
                        ApplyTheme(settings, theme);
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
                    const auto refreshedPipboyColor = view.resources.pipboyColor;
                    if (refreshedPipboyColor.valid) {
                        ApplyPipboyColor(settings, refreshedPipboyColor);
                    }
                }
                changed = true;
            }
            if (settings.syncPipboyColor) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", L("Settings", "sAuto", ""));
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
                ApplyTheme(settings, DefaultTheme);
                settings.syncPipboyColor = false;
                changed = true;
            }
            blockSpacing();
            if (ImGui::Button(L("Settings", "sRefreshThemes", "Refresh Themes"), ImVec2(buttonRowWidth(), 0.0f))) {
                requests.reloadThemes = true;
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
            ImGui::TextDisabled("%s: %s", L("Settings", "sGamepadStatus", "Gamepad"), view.resources.gamepadConnected ? L("Settings", "sConnected", "Connected") : L("Settings", "sDisconnected", "Disconnected"));
            ImGui::TextDisabled("%s: %s", L("Settings", "sControllerToggle", "Toggle"), L("Settings", "sToggleCombo", "Back + Start"));
            ImGui::TextDisabled("%s: %s / %s", L("Settings", "sNavigation", "Navigation"), L("Settings", "sDPad", "D-Pad"), L("Settings", "sLeftStick", "Left Stick"));
            ImGui::TextDisabled("%s: %s  |  %s: %s", L("Settings", "sConfirm", "Confirm"), L("Settings", "sButtonA", "A"), L("Settings", "sGoBack", "Back"), L("Settings", "sButtonB", "B"));
            ImGui::TextDisabled("%s: %s", L("Settings", "sTabSwitch", "Tab Switch"), L("Settings", "sShoulderButtons", "Shoulder Buttons"));
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsFavoritesSection", L("General", "sFavorites", "Favorites"))) {
            sectionSpacing();
            FavoritesPanel::Draw(state.favorites, view.favorites, view.localize, requests.favorites);
            sectionSpacing();
            ImGui::TreePop();
        }

        ImGui::Spacing();

        if (BeginSection("SettingsLoggingSection", L("Settings", "sLoggingSection", "Logging"))) {
            sectionSpacing();
            changed = ImGui::Checkbox(L("Settings", "sShowLogsTab", "Show Logs Tab"), &settings.showLogsTab) || changed;
            if (ImGui::Checkbox(L("Settings", "sDebugLogging", "Debug Logging"), &settings.debugLogging)) {
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
                settings = Settings{};
                requests.resetAll = true;
                state.waitingForToggleKey = false;
                currentLanguage = settings.language;
                changed = true;
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
        ImGui::TextDisabled("%s: %s", L("Settings", "sGameVersion", "Game Version"), (view.resources.gameVersion.empty() ? L("General", "sUnknown", "Unknown") : view.resources.gameVersion.c_str()));
        ImGui::TextDisabled("%s: %s", L("Settings", "sModVersion", "Mod Version"), view.resources.modVersion.c_str());
        ImGui::Spacing();
        sectionSpacing();

        const float aboutButtonWidth = dualButtonWidth();
        if (ImGui::Button(L("Settings", "sOpenNexusMods", "Open Nexus Mods Page"), ImVec2(aboutButtonWidth, 0.0f))) {
            requests.page = SettingsPage::NexusMods;
        }
        ImGui::SameLine();
        if (ImGui::Button(L("Settings", "sOpenGitHub", "GitHub"), ImVec2(aboutButtonWidth, 0.0f))) {
            requests.page = SettingsPage::GitHub;
        }
        if (ImGui::Button(L("Settings", "sOpenBugReport", "Report a Bug"), ImVec2(aboutButtonWidth, 0.0f))) {
            requests.page = SettingsPage::BugReport;
        }
        ImGui::SameLine();
        if (ImGui::Button(L("Settings", "sOpenBuyMeACoffee", "Buy Me A Coffee"), ImVec2(aboutButtonWidth, 0.0f))) {
            requests.page = SettingsPage::Donations;
        }
        if (ImGui::Button(L("Settings", "sShowHelpOverlay", "Show Help Overlay"), ImVec2(buttonRowWidth(), 0.0f))) {
            requests.showHelp = true;
        }
        sectionSpacing();

        ImGui::PopStyleVar(3);

        if (changed) requests.settings = std::move(settings);

        ImGui::EndChild();
        }
        };
    }

    void SettingsTab::Draw(SettingsTabState& state, const SettingsTabView& view, SettingsTabRequests& requests)
    {
        SettingsView(state, view, requests).Draw();
    }
}
