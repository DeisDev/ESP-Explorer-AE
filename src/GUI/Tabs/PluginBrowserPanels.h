#pragma once

#include "GUI/Tabs/PluginBrowserHelpers.h"

namespace ESPExplorerAE::PluginBrowserPanels
{
    void DrawTreePane(const std::vector<PluginInfo>& plugins, const CatalogSnapshot& cache, PluginBrowserHelpers::Context& context, float leftWidth);
    void DrawDetailsPane(const std::vector<PluginInfo>& plugins, const CatalogSnapshot& cache, PluginBrowserHelpers::Context& context);
}