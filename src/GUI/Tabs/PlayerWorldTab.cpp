#include "GUI/Tabs/PlayerWorldTab.h"

#include "GUI/Widgets/BrowserWidgets.h"
#include "GUI/Widgets/ActionFeedback.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void DrawPlayerWorld(PlayerWorldState& state, const BrowserView& view, PlayerStatus player, bool godMode, BrowserRequests& requests)
    {
        const auto& localize = view.localize;
        const auto L = [&](const char* key, const char* fallback) { return localize("Inventory", key, fallback); };
        const auto emit = [&](ActionRequest request) {
            request.session = view.session;
            if (view.gameplayReady && ActionQueue::Valid(request)) requests.actions.push_back({std::move(request), false, {}});
        };
        ActionFeedback::Draw(state.admission, localize);
        if (player.ready && player.session == view.session) ImGui::Text("%s: %d | %s: %.0f | %s: %.0f", localize("General", "sLevel", "Level"), player.level,
            L("sHealthShort", "HP"), player.health, L("sActionPointsShort", "AP"), player.actionPoints);
        ImGui::BeginDisabled(!view.gameplayReady);
        if (ImGui::Button(L("sRefillHealth", "Refill Health"))) emit({.kind = ActionKind::RestoreHealth});
        ImGuiWidgetUtils::DrawWrappedSameLine(L("sToggleNoClip", "Toggle Noclip"));
        if (ImGui::Button(L("sToggleNoClip", "Toggle Noclip"))) emit({.kind = ActionKind::ToggleNoClip});
        const auto* godLabel = godMode ? L("sGodModeOn", "Godmode: ON") : L("sGodModeOff", "Godmode: OFF");
        ImGuiWidgetUtils::DrawWrappedSameLine(godLabel);
        if (ImGui::Button(godLabel)) emit({.kind = ActionKind::GodMode, .ammoCount = godMode ? 0u : 1u});
        ImGui::SetNextItemWidth(180);
        ImGui::InputInt(L("sSetLevel", "Set Level"), &state.level, 1, 10);
        state.level = std::clamp(state.level, 1, 65535);
        if (ImGui::Button(L("sApplyLevel", "Apply Level"))) emit({.kind = ActionKind::SetPlayerLevel, .count = static_cast<std::uint32_t>(state.level)});
        ImGui::SetNextItemWidth(180);
        ImGui::InputInt(L("sAddPerkPoints", "Perk Points"), &state.perkPoints, 1, 5);
        state.perkPoints = std::clamp(state.perkPoints, 1, 999);
        if (ImGui::Button(L("sAddPerkPointsBtn", "Add Perk Points"))) emit({.kind = ActionKind::AddPerkPoints, .count = static_cast<std::uint32_t>(state.perkPoints)});
        ImGui::SeparatorText(L("sTimeOfDaySection", "Time of Day"));
        ImGui::SetNextItemWidth(360);
        ImGui::SliderFloat(L("sTimeOfDaySlider", "Hour"), &state.gameHour, 0, 23.99f, "%.2f");
        bool first = true;
        if (ImGuiWidgetUtils::DrawWrappedButton(L("sTimeMorning", "Morning"), first)) state.gameHour = 6;
        if (ImGuiWidgetUtils::DrawWrappedButton(L("sTimeNoon", "Noon"), first)) state.gameHour = 12;
        if (ImGuiWidgetUtils::DrawWrappedButton(L("sTimeEvening", "Evening"), first)) state.gameHour = 18;
        if (ImGuiWidgetUtils::DrawWrappedButton(L("sTimeMidnight", "Midnight"), first)) state.gameHour = 0;
        if (ImGuiWidgetUtils::DrawWrappedButton(L("sApplyTime", "Set Time"), first)) emit({.kind = ActionKind::SetGameHour, .value = state.gameHour});
        ImGui::SeparatorText(localize("General", "sSetWeather", "Set Weather"));
        ImGui::SetNextItemWidth(360);
        ImGui::InputTextWithHint("##WeatherSearch", localize("General", "sSearch", "Search"), state.weatherSearch.data(), state.weatherSearch.size());
        SearchBar::ReadControllerText(localize("General", "sSearch", "Search"), state.weatherSearch.data(), state.weatherSearch.size());
        const auto* selected = view.catalog->Find(state.weather);
        ImGui::SetNextItemWidth(360);
        if (ImGui::BeginCombo("##Weather", selected ? selected->name.c_str() : localize("General", "sNone", "None"))) {
            if (const auto found = view.catalog->byType.find("WTHR"); found != view.catalog->byType.end()) for (const auto index : found->second) {
                const auto& weather = view.catalog->records[index];
                if (!SearchContains(weather.name, state.weatherSearch.data()) && !SearchContains(weather.editorID, state.weatherSearch.data())) continue;
                const auto label = (weather.name.empty() ? weather.editorID : weather.name) + "###" + std::to_string(weather.formID);
                if (ImGui::Selectable(label.c_str(), state.weather == weather.formID)) state.weather = weather.formID;
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(!selected || selected->isDeleted);
        if (ImGui::Button(localize("General", "sSetWeather", "Set Weather")) && selected) BrowserWidgets::Emit(requests, view, *selected, ActionKind::SetWeather);
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (!view.gameplayReady) ImGui::TextWrapped("%s", localize("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open."));
    }
}
