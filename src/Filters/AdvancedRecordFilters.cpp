#include "Filters/AdvancedRecordFilters.h"



#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <regex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace ESPExplorerAE
{
    namespace
    {
        std::string PercentEncode(std::string_view value)
        {
            std::string encoded{};
            encoded.reserve(value.size());

            char buffer[4]{};
            for (const unsigned char ch : value) {
                if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
                    encoded.push_back(static_cast<char>(ch));
                    continue;
                }

                std::snprintf(buffer, sizeof(buffer), "%%%02X", ch);
                encoded.append(buffer);
            }

            return encoded;
        }

        std::string PercentDecode(std::string_view value)
        {
            std::string decoded{};
            decoded.reserve(value.size());

            for (std::size_t index = 0; index < value.size(); ++index) {
                if (value[index] == '%' && index + 2 < value.size()) {
                    unsigned int decodedByte = 0;
                    const auto hex = value.substr(index + 1, 2);
                    const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), decodedByte, 16);
                    if (ec == std::errc{} && ptr == hex.data() + hex.size()) {
                        decoded.push_back(static_cast<char>(decodedByte));
                        index += 2;
                        continue;
                    }
                }

                decoded.push_back(value[index]);
            }

            return decoded;
        }

        bool TryParseRuleField(std::string_view value, AdvancedFilterField& field)
        {
            int parsed = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (ec != std::errc{} || ptr != value.data() + value.size() || parsed < 0 || parsed > static_cast<int>(AdvancedFilterField::Keyword)) {
                return false;
            }

            field = static_cast<AdvancedFilterField>(parsed);
            return true;
        }

        bool TryParseRuleMatch(std::string_view value, AdvancedFilterMatch& match)
        {
            int parsed = 0;
            const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (ec != std::errc{} || ptr != value.data() + value.size() || parsed < 0 || parsed > static_cast<int>(AdvancedFilterMatch::Regex)) {
                return false;
            }

            match = static_cast<AdvancedFilterMatch>(parsed);
            return true;
        }

    }

    const std::vector<AdvancedFilterRule>& AdvancedRecordFilters::GetDefaultRules()
    {
        static const std::vector<AdvancedFilterRule> defaults{
            { true, AdvancedFilterField::Keyword, AdvancedFilterMatch::Exact, "SS2_Tag_LevelSkin" },
            { true, AdvancedFilterField::Keyword, AdvancedFilterMatch::Exact, "SS2_Tag_BuildingPlan" },
            { true, AdvancedFilterField::Keyword, AdvancedFilterMatch::Exact, "SS2_Tag_LevelPlan" },
            { true, AdvancedFilterField::Keyword, AdvancedFilterMatch::Exact, "SS2_Tag_BuildingSkin" }
        };

        return defaults;
    }

    std::vector<AdvancedFilterRule> AdvancedRecordFilters::LoadRules(std::string_view serialized)
    {
        if (serialized.empty()) {
            return GetDefaultRules();
        }

        std::vector<AdvancedFilterRule> rules{};
        std::size_t start = 0;

        while (start <= serialized.size()) {
            const auto end = serialized.find(';', start);
            const auto line = serialized.substr(start, end == std::string_view::npos ? std::string_view::npos : (end - start));

            if (!line.empty()) {
                std::vector<std::string_view> tokens{};
                std::size_t tokenStart = 0;
                while (tokenStart < line.size()) {
                    const auto tokenEnd = line.find('|', tokenStart);
                    tokens.push_back(line.substr(tokenStart, tokenEnd == std::string_view::npos ? std::string_view::npos : (tokenEnd - tokenStart)));
                    if (tokenEnd == std::string_view::npos) {
                        break;
                    }
                    tokenStart = tokenEnd + 1;
                }

                if (tokens.size() >= 4) {
                    AdvancedFilterRule rule{};
                    rule.enabled = tokens[0] == "1";
                    bool valid = TryParseRuleField(tokens[1], rule.field) && TryParseRuleMatch(tokens[2], rule.match);
                    rule.value = PercentDecode(tokens[3]);

                    if (tokens.size() >= 5 && !tokens[4].empty()) {
                        std::size_t pStart = 0;
                        while (pStart < tokens[4].size()) {
                            const auto pEnd = tokens[4].find(',', pStart);
                            const auto pluginToken = tokens[4].substr(pStart, pEnd == std::string_view::npos ? std::string_view::npos : (pEnd - pStart));
                            auto decoded = PercentDecode(pluginToken);
                            if (!decoded.empty()) {
                                rule.targetPlugins.push_back(std::move(decoded));
                            }
                            if (pEnd == std::string_view::npos) {
                                break;
                            }
                            pStart = pEnd + 1;
                        }
                    }

                    if (valid && !rule.value.empty()) {
                        rules.push_back(std::move(rule));
                    }
                }
            }

            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }

        if (rules.empty()) {
            return GetDefaultRules();
        }

        return rules;
    }

    std::string AdvancedRecordFilters::SaveRules(const std::vector<AdvancedFilterRule>& rules)
    {
        std::string serialized{};

        for (const auto& rule : rules) {
            if (rule.value.empty()) {
                continue;
            }

            if (!serialized.empty()) {
                serialized.push_back(';');
            }

            serialized += rule.enabled ? '1' : '0';
            serialized.push_back('|');
            serialized += std::to_string(static_cast<int>(rule.field));
            serialized.push_back('|');
            serialized += std::to_string(static_cast<int>(rule.match));
            serialized.push_back('|');
            serialized += PercentEncode(rule.value);
            serialized.push_back('|');
            for (std::size_t i = 0; i < rule.targetPlugins.size(); ++i) {
                if (i > 0) {
                    serialized.push_back(',');
                }
                serialized += PercentEncode(rule.targetPlugins[i]);
            }
        }

        return serialized;
    }

    bool AdvancedRecordFilters::IsRegexValid(std::string_view pattern)
    {
        if (pattern.empty()) {
            return false;
        }

        try {
            std::regex compiled(std::string(pattern), std::regex_constants::icase | std::regex_constants::optimize);
            return true;
        } catch (const std::regex_error&) {
            return false;
        }
    }

    std::size_t AdvancedRecordFilters::CountActiveRules(const std::vector<AdvancedFilterRule>& rules)
    {
        return std::ranges::count_if(rules, [](const AdvancedFilterRule& rule) {
            return rule.enabled && !rule.value.empty();
        });
    }

    std::unordered_set<std::string> AdvancedRecordFilters::LoadHiddenPlugins(std::string_view serialized)
    {
        std::unordered_set<std::string> result{};
        if (serialized.empty()) {
            return result;
        }

        std::size_t start = 0;
        while (start <= serialized.size()) {
            const auto end = serialized.find(';', start);
            const auto token = serialized.substr(start, end == std::string_view::npos ? std::string_view::npos : (end - start));
            auto decoded = PercentDecode(token);
            if (!decoded.empty()) {
                result.insert(std::move(decoded));
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }

        return result;
    }

    std::string AdvancedRecordFilters::SaveHiddenPlugins(const std::unordered_set<std::string>& plugins)
    {
        std::string serialized{};
        std::vector<std::string> sorted(plugins.begin(), plugins.end());
        std::sort(sorted.begin(), sorted.end());

        for (const auto& plugin : sorted) {
            if (!serialized.empty()) {
                serialized.push_back(';');
            }
            serialized += PercentEncode(plugin);
        }

        return serialized;
    }
}
