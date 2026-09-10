#pragma once

#include "GUI/BrowserState.h"

namespace ESPExplorerAE::BrowserWidgets
{
    void DrawControls(BrowserState& state, const BrowserView& view, BrowserRequests& requests,
        const char* id, const char* section, const char* searchFallback);
    CatalogQuery MakeQuery(const BrowserState& state, const BrowserView& view, const BrowserCategoryState& rows, std::string type);
    void Emit(BrowserRequests& requests, const BrowserView& view, const FormEntry& entry,
        ActionKind kind, bool confirm = false, std::uint32_t count = 1);
    enum class ContextScope { Single, Selection, ActiveInSelection };
    void DrawContext(const FormEntry& entry, ContextScope scope, std::unordered_map<std::uint32_t, int>& quantities,
        const BrowserView& view, BrowserRequests& requests);
    void DrawCategory(BrowserState& state, const BrowserView& view, BrowserRequests& requests,
        const char* category, const FormTableConfig& config, ActionKind primary, std::optional<ActionKind> secondary = {},
        std::span<const std::string> types = {});
    void ActivateCategory(BrowserState& state, const BrowserView& view, const char* type);
}
