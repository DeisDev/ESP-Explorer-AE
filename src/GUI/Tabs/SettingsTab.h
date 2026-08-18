#pragma once
#include "GUI/Widgets/FavoritesPanel.h"
#include "Core/SettingsResources.h"

namespace ESPExplorerAE
{
    struct SettingsTabState
    {
        bool waitingForToggleKey{};
        FavoritesPanelState favorites;
        void Close() { waitingForToggleKey = false; favorites.selected.clear(); }
    };

    struct SettingsTabView
    {
        const Settings& settings;
        const SettingsResources& resources;
        const FavoriteReview& favorites;
        FavoritesPanel::Localize localize;
    };

    struct SettingsTabRequests
    {
        std::optional<Settings> settings;
        FavoritesPanelRequests favorites;
        std::optional<SettingsPage> page;
        bool resetAll{};
        bool reloadThemes{};
        bool showHelp{};
    };

    class SettingsTab
    {
    public:
        static void Draw(SettingsTabState& state, const SettingsTabView& view, SettingsTabRequests& requests);
    };
}
