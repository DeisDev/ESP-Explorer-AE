#include "GUI/Widgets/FavoritesPanel.h"
#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        const char* Reason(FavoriteResolution status, const FavoritesPanel::Localize& localize)
        {
            switch (status) {
            case FavoriteResolution::Unavailable: return localize("Favorites", "sUnavailable", "Catalog unavailable");
            case FavoriteResolution::InvalidKey: return localize("Favorites", "sInvalid", "Unrecognized entry");
            case FavoriteResolution::MissingPlugin: return localize("Favorites", "sMissingPlugin", "Plugin not loaded");
            case FavoriteResolution::AmbiguousPlugin: return localize("Favorites", "sAmbiguous", "Plugin identity is ambiguous");
            case FavoriteResolution::ChangedPluginKind: return localize("Favorites", "sChangedKind", "Plugin full/light type changed");
            case FavoriteResolution::MissingRecord: return localize("Favorites", "sMissingRecord", "Record not found");
            case FavoriteResolution::TemporaryRecord: return localize("Favorites", "sTemporary", "Temporary record; cannot be migrated");
            default: return "";
            }
        }
    }

    void FavoritesPanel::Draw(FavoritesPanelState& state, const FavoriteReview& review, const Localize& localize, FavoritesPanelRequests& requests)
    {
        std::vector<std::string> tokens;
        for (const auto& entry : review.legacy) tokens.push_back(entry.token);
        if (state.generation != review.generation || state.session != review.session || state.tokens != tokens) {
            state.stale = !state.selected.empty();
            state.selected.clear();
            state.generation = review.generation;
            state.session = review.session;
            state.tokens = std::move(tokens);
        }
        ImGui::TextWrapped("%s", localize("Favorites", "sPersistence", "Favorites use plugin-relative identities. Unavailable entries are kept for when their plugins return."));
        if (review.temporaryCount) ImGui::TextWrapped("%s: %zu", localize("Favorites", "sSessionOnly", "Favorites kept only for this session"), review.temporaryCount);
        if (state.stale) ImGui::TextWrapped("%s", localize("Favorites", "sReviewChanged", "The favorite review changed. Select the entries again."));
        if (!review.legacy.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", localize("Favorites", "sLegacyReview", "Legacy IDs do not store their original plugins. Review the current matches below. Select only records you recognize; accepting them uses the current load order. Other entries are retained. The original INI is backed up before the first save."));
            for (std::size_t index = 0; index < review.legacy.size(); ++index) {
                const auto& entry = review.legacy[index];
                ImGui::PushID(static_cast<int>(index));
                bool selected = state.selected.contains(index);
                ImGui::BeginDisabled(!entry.target || !review.ready);
                const auto label = entry.token + "###LegacyFavorite";
                if (ImGui::Checkbox(label.c_str(), &selected)) {
                    if (selected) state.selected.insert(index); else state.selected.erase(index);
                    state.stale = false;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (entry.target && entry.target.key) ImGui::TextWrapped("%s | %s | %08X", entry.name.c_str(), entry.target.key->plugin.c_str(), entry.target.key->localID);
                else ImGui::TextWrapped("%s", Reason(entry.target.status, localize));
                ImGui::PopID();
            }
            ImGui::BeginDisabled(state.selected.empty() || !review.ready);
            if (ImGui::Button((std::string(localize("Favorites", "sAcceptCurrent", "Use Selected Current Records")) + "###AcceptLegacyFavorites").c_str())) {
                requests.review = review;
                requests.accepted.assign(state.selected.begin(), state.selected.end());
                std::ranges::sort(requests.accepted);
            }
            ImGui::EndDisabled();
        }
        if (!review.unresolved.empty()) {
            ImGui::Separator();
            ImGui::TextUnformatted(localize("Favorites", "sUnresolved", "Unresolved Saved Favorites"));
            for (const auto& entry : review.unresolved) ImGui::TextWrapped("%s: %s", entry.token.c_str(), Reason(entry.target.status, localize));
        }
    }
}
