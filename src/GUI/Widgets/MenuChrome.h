#pragma once
#include <array>
#include <functional>
#include <span>
#include <string_view>

namespace ESPExplorerAE::MenuChrome
{
    // Drawing helpers consume frame-owned labels and keep no application state.
    bool Header(const char* title, const char* subtitle, const char* closeLabel);
    void Section(const char* label);
    bool NavigationItem(const char* id, const char* label, const char* count, const char* icon, bool selected);
    void PageHeading(const char* label, const char* count);
    int Destinations(const std::array<const char*, 4>& labels, int selected);
    struct ToolbarPage { const char* id; const char* label; bool selected; };
    struct ToolbarRequests
    {
        std::string_view page;
        bool commands{}, basket{}, compare{}, sources{}, inspector{}, diagnostics{}, refresh{}, reset{};
    };
    ToolbarRequests WorkspaceToolbar(std::span<const ToolbarPage> pages, std::size_t basketCount,
        bool explore, bool sourcesShown, bool inspectorShown, bool refreshing,
        const std::function<const char*(std::string_view, std::string_view, const char*)>& localize);
}
