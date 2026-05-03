#include "Filters/AdvancedRecordFilters.h"

#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/SharedUtils.h"

#include <RE/B/BGSKeyword.h>
#include <RE/B/BGSKeywordForm.h>
#include <RE/T/TESDataHandler.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <regex>
#include <unordered_map>
#include <unordered_set>

namespace ESPExplorerAE
{
    namespace
    {
        struct CompiledRule
        {
            AdvancedFilterRule rule{};
            bool regexValid{ false };
            std::regex regex{};
        };

        struct CompiledRuleCache
        {
            std::string serialized{};
            std::vector<CompiledRule> rules{};
        };

        struct FieldCacheState
        {
            std::uint64_t dataVersion{ 0 };
            std::unordered_map<std::uint32_t, std::string> editorIDs{};
            std::unordered_map<std::uint32_t, std::vector<std::string>> keywords{};
            std::vector<std::string> availableKeywords{};
        };

        CompiledRuleCache compiledCache{};
        FieldCacheState fieldCache{};

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

        void ResetCachesIfNeeded()
        {
            const auto dataVersion = DataManager::GetDataVersion();
            if (fieldCache.dataVersion == dataVersion) {
                return;
            }

            fieldCache.dataVersion = dataVersion;
            fieldCache.editorIDs.clear();
            fieldCache.keywords.clear();
            fieldCache.availableKeywords.clear();
        }

        const std::string& GetEditorID(std::uint32_t formID)
        {
            ResetCachesIfNeeded();

            auto [it, inserted] = fieldCache.editorIDs.try_emplace(formID);
            if (inserted) {
                if (const char* editorID = ContextMenu::TryGetEditorID(formID)) {
                    it->second = editorID;
                }
            }

            return it->second;
        }

        const std::vector<std::string>& GetKeywords(std::uint32_t formID)
        {
            ResetCachesIfNeeded();

            auto [it, inserted] = fieldCache.keywords.try_emplace(formID);
            if (!inserted) {
                return it->second;
            }

            auto* form = RE::TESForm::GetFormByID(formID);
            const auto* keywordForm = form ? form->As<RE::BGSKeywordForm>() : nullptr;
            if (!keywordForm) {
                return it->second;
            }

            std::unordered_set<std::string> seen{};
            keywordForm->ForEachKeyword([&](RE::BGSKeyword* keyword) {
                if (!keyword) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                const auto* editorID = keyword->formEditorID.c_str();
                if (!editorID || editorID[0] == '\0') {
                    return RE::BSContainer::ForEachResult::kContinue;
                }

                if (seen.emplace(editorID).second) {
                    it->second.emplace_back(editorID);
                }

                return RE::BSContainer::ForEachResult::kContinue;
            });

            std::sort(it->second.begin(), it->second.end());
            return it->second;
        }

        bool MatchesValue(std::string_view candidate, const CompiledRule& compiledRule)
        {
            if (candidate.empty()) {
                return false;
            }

            switch (compiledRule.rule.match) {
            case AdvancedFilterMatch::Contains:
                return SharedUtils::ContainsCaseInsensitive(candidate, compiledRule.rule.value);
            case AdvancedFilterMatch::Exact:
                return SharedUtils::EqualsCaseInsensitive(candidate, compiledRule.rule.value);
            case AdvancedFilterMatch::Regex:
                if (!compiledRule.regexValid) {
                    return false;
                }
                return std::regex_search(candidate.begin(), candidate.end(), compiledRule.regex);
            }

            return false;
        }

        bool MatchesAnyValue(const CompiledRule& compiledRule, std::initializer_list<std::string_view> values)
        {
            for (const auto value : values) {
                if (MatchesValue(value, compiledRule)) {
                    return true;
                }
            }

            return false;
        }

        const std::vector<CompiledRule>& GetCompiledRules(const std::vector<AdvancedFilterRule>& rules)
        {
            const auto serialized = AdvancedRecordFilters::SaveRules(rules);
            if (compiledCache.serialized == serialized) {
                return compiledCache.rules;
            }

            compiledCache.serialized = serialized;
            compiledCache.rules.clear();
            compiledCache.rules.reserve(rules.size());

            for (const auto& rule : rules) {
                if (rule.value.empty()) {
                    continue;
                }

                CompiledRule compiledRule{};
                compiledRule.rule = rule;

                if (compiledRule.rule.match == AdvancedFilterMatch::Regex) {
                    try {
                        compiledRule.regex = std::regex(compiledRule.rule.value, std::regex_constants::icase | std::regex_constants::optimize);
                        compiledRule.regexValid = true;
                    } catch (const std::regex_error&) {
                        compiledRule.regexValid = false;
                    }
                }

                compiledCache.rules.push_back(std::move(compiledRule));
            }

            return compiledCache.rules;
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

    bool AdvancedRecordFilters::Passes(const FormEntry& entry, const std::vector<AdvancedFilterRule>& rules)
    {
        const auto& compiledRules = GetCompiledRules(rules);
        if (compiledRules.empty()) {
            return true;
        }

        const auto& editorID = GetEditorID(entry.formID);
        const auto& keywords = GetKeywords(entry.formID);

        for (const auto& compiledRule : compiledRules) {
            if (!compiledRule.rule.enabled) {
                continue;
            }

            if (!compiledRule.rule.targetPlugins.empty()) {
                bool pluginMatch = std::ranges::any_of(compiledRule.rule.targetPlugins, [&](const std::string& plugin) {
                    return SharedUtils::EqualsCaseInsensitive(entry.sourcePlugin, plugin);
                });
                if (!pluginMatch) {
                    continue;
                }
            }

            bool matched = false;
            switch (compiledRule.rule.field) {
            case AdvancedFilterField::Any:
                matched = MatchesAnyValue(compiledRule, { entry.name, editorID, entry.sourcePlugin, entry.category });
                if (!matched) {
                    matched = std::ranges::any_of(keywords, [&](const std::string& keyword) {
                        return MatchesValue(keyword, compiledRule);
                    });
                }
                break;
            case AdvancedFilterField::Name:
                matched = MatchesValue(entry.name, compiledRule);
                break;
            case AdvancedFilterField::EditorID:
                matched = MatchesValue(editorID, compiledRule);
                break;
            case AdvancedFilterField::Plugin:
                matched = MatchesValue(entry.sourcePlugin, compiledRule);
                break;
            case AdvancedFilterField::Category:
                matched = MatchesValue(entry.category, compiledRule);
                break;
            case AdvancedFilterField::Keyword:
                matched = std::ranges::any_of(keywords, [&](const std::string& keyword) {
                    return MatchesValue(keyword, compiledRule);
                });
                break;
            }

            if (matched) {
                return false;
            }
        }

        return true;
    }

    const std::vector<std::string>& AdvancedRecordFilters::GetAvailableKeywords()
    {
        ResetCachesIfNeeded();
        if (!fieldCache.availableKeywords.empty()) {
            return fieldCache.availableKeywords;
        }

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return fieldCache.availableKeywords;
        }

        std::unordered_set<std::string> seen{};
        const auto& keywordForms = dataHandler->GetFormArray<RE::BGSKeyword>();
        fieldCache.availableKeywords.reserve(keywordForms.size());

        for (auto* keyword : keywordForms) {
            if (!keyword) {
                continue;
            }

            const auto* editorID = keyword->formEditorID.c_str();
            if (!editorID || editorID[0] == '\0') {
                continue;
            }

            if (seen.emplace(editorID).second) {
                fieldCache.availableKeywords.emplace_back(editorID);
            }
        }

        std::sort(fieldCache.availableKeywords.begin(), fieldCache.availableKeywords.end());
        return fieldCache.availableKeywords;
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

    const std::vector<std::string>& AdvancedRecordFilters::GetAvailablePlugins()
    {
        ResetCachesIfNeeded();
        static std::vector<std::string> cachedPlugins{};
        static std::uint64_t cachedVersion{ 0 };

        const auto dataVersion = DataManager::GetDataVersion();
        if (cachedVersion == dataVersion && !cachedPlugins.empty()) {
            return cachedPlugins;
        }

        cachedVersion = dataVersion;
        cachedPlugins.clear();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return cachedPlugins;
        }

        for (auto* file : dataHandler->compiledFileCollection.files) {
            if (!file) continue;
            const auto filename = file->GetFilename();
            if (!filename.empty()) {
                cachedPlugins.emplace_back(filename);
            }
        }
        for (auto* file : dataHandler->compiledFileCollection.smallFiles) {
            if (!file) continue;
            const auto filename = file->GetFilename();
            if (!filename.empty()) {
                cachedPlugins.emplace_back(filename);
            }
        }

        std::sort(cachedPlugins.begin(), cachedPlugins.end(), [](const std::string& a, const std::string& b) {
            return _stricmp(a.c_str(), b.c_str()) < 0;
        });

        return cachedPlugins;
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
