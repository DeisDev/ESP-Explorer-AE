#pragma once

#include "GUI/BrowserState.h"
#include "GUI/Tabs/PluginBrowserState.h"
#include "Core/CopyFormat.h"
#include "Core/RecordDetails.h"

namespace ESPExplorerAE
{
    struct PluginBrowserView
    {
        BrowserView records;
        bool advancedDetails{};
        std::shared_ptr<const RecordDetails> details;
        std::size_t recentLimit{ 30 };
        MultiCopyFormat copyFormat{ MultiCopyFormat::Lines };
        std::size_t favoriteReviewCount{};
        std::function<void()> drawInspector;
        bool showInspector{true};
        float inspectorWidth{380.0f};
    };

    struct PluginBrowserRequests
    {
        BrowserRequests records;
        std::optional<std::string> pluginFilter;
        bool settingsChanged{};
        std::optional<DetailKey> details;
        std::optional<float> resultsWidth;
    };

    class PluginBrowserTab
    {
    public:
        static void Draw(PluginBrowserState& state, const PluginBrowserView& view, PluginBrowserRequests& requests);
    };
}
