#pragma once

#include "Filters/AdvancedRecordFilters.h"
#include "Core/CatalogSnapshot.h"
#include "Core/CatalogQuery.h"

#include <functional>
#include <optional>
#include <unordered_set>

namespace ESPExplorerAE
{
    struct RecordFilterState
    {
        bool& showNonPlayable;
        bool& showUnnamed;
        bool& showDeleted;
        std::vector<AdvancedFilterRule>& advancedRules;
        std::unordered_set<std::string>& hiddenPlugins;
    };

    struct AdvancedFilterEditorState
    {
        struct UndoState
        {
            std::vector<AdvancedFilterRule> rules;
            std::unordered_set<std::string> hiddenPlugins;
        };

        bool open{};
        bool reopenAfterMenuShow{};
        bool focusPending{};
        bool menuVisible{ true };
        int newField{ static_cast<int>(AdvancedFilterField::Any) };
        int newMatch{ static_cast<int>(AdvancedFilterMatch::Contains) };
        char newValue[256]{};
        char keywordSearch[128]{};
        char hiddenPluginSearch[128]{};
        char ruleScopeSearch[128]{};
        char ruleSearch[256]{};
        char hiddenFormID[9]{};
        std::vector<std::string> newTargetPlugins;
        std::optional<UndoState> undo;
        std::optional<UndoState> undoResult;
        std::optional<AdvancedFilterRule> previewRule;
        std::size_t previewCount{};
        std::vector<RecordIndex> previewSamples;
        std::shared_ptr<const CatalogSnapshot> catalog;
        std::vector<std::size_t> pluginOrder;

        void UpdateChoices(std::shared_ptr<const CatalogSnapshot> snapshot);
        void HandleMenuVisibilityChanged(bool visible);
    };

    class RecordFiltersWidget
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        static bool Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state,
            AdvancedFilterEditorState& editorState, std::shared_ptr<const CatalogSnapshot> catalog);
        static bool DrawEditor(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state,
            AdvancedFilterEditorState& editorState, std::shared_ptr<const CatalogSnapshot> catalog);
        static void DrawWhyHidden(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state,
            AdvancedFilterEditorState& editorState, const CatalogQuery& query, std::shared_ptr<const CatalogSnapshot> catalog);
    };
}
