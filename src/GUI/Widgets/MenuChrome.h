#pragma once

namespace ESPExplorerAE::MenuChrome
{
    // Drawing helpers consume frame-owned labels and keep no application state.
    bool Header(const char* title, const char* subtitle, const char* closeLabel);
    void Section(const char* label);
    bool NavigationItem(const char* id, const char* label, const char* count, const char* icon, bool selected);
    void PageHeading(const char* label, const char* count);
}
