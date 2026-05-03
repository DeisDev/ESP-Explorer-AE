#pragma once

#include "Data/DataManager.h"

#include <string>
#include <string_view>
#include <vector>

namespace ESPExplorerAE
{
    enum class AdvancedFilterField
    {
        Any = 0,
        Name = 1,
        EditorID = 2,
        Plugin = 3,
        Category = 4,
        Keyword = 5
    };

    enum class AdvancedFilterMatch
    {
        Contains = 0,
        Exact = 1,
        Regex = 2
    };

    struct AdvancedFilterRule
    {
        bool enabled{ true };
        AdvancedFilterField field{ AdvancedFilterField::Any };
        AdvancedFilterMatch match{ AdvancedFilterMatch::Contains };
        std::string value{};
        std::vector<std::string> targetPlugins{};
    };

    class AdvancedRecordFilters
    {
    public:
        static const std::vector<AdvancedFilterRule>& GetDefaultRules();
        static std::vector<AdvancedFilterRule> LoadRules(std::string_view serialized);
        static std::string SaveRules(const std::vector<AdvancedFilterRule>& rules);
        static bool Passes(const FormEntry& entry, const std::vector<AdvancedFilterRule>& rules);
        static const std::vector<std::string>& GetAvailableKeywords();
        static const std::vector<std::string>& GetAvailablePlugins();
        static bool IsRegexValid(std::string_view pattern);
        static std::size_t CountActiveRules(const std::vector<AdvancedFilterRule>& rules);
        static std::unordered_set<std::string> LoadHiddenPlugins(std::string_view serialized);
        static std::string SaveHiddenPlugins(const std::unordered_set<std::string>& plugins);
    };
}
