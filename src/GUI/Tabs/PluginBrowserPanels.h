#pragma once

#include "GUI/Tabs/PluginBrowserTab.h"

namespace ESPExplorerAE::PluginBrowserPanels
{
    void DrawTreePane(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context, float leftWidth);
    void DrawDetailsPane(const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context);
}