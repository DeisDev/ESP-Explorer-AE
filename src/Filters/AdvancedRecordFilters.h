#pragma once

#include "Core/FilterRule.h"
#include "Core/FormEntry.h"
#include <unordered_set>

#include <string>
#include <string_view>
#include <vector>

namespace ESPExplorerAE
{
    class AdvancedRecordFilters
    {
    public:
        static const std::vector<AdvancedFilterRule>& GetDefaultRules();
        static std::vector<AdvancedFilterRule> LoadRules(std::string_view serialized);
        static std::string SaveRules(const std::vector<AdvancedFilterRule>& rules);
        static bool IsRegexValid(std::string_view pattern);
        static std::size_t CountActiveRules(const std::vector<AdvancedFilterRule>& rules);
        static std::unordered_set<std::string> LoadHiddenPlugins(std::string_view serialized);
        static std::string SaveHiddenPlugins(const std::unordered_set<std::string>& plugins);
    };
}
