#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/FilterRule.h"
#include "Core/NPCFilters.h"
#include "Core/RecordColumns.h"
#include "Core/SearchQuery.h"

#include <array>
#include <cstdio>
#include <optional>
#include <regex>
#include <span>
#include <unordered_set>

namespace ESPExplorerAE
{
    inline unsigned char FoldASCII(unsigned char ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; }
    inline bool TextEquals(std::string_view left, std::string_view right)
    {
        return std::ranges::equal(left, right, [](unsigned char a, unsigned char b) { return FoldASCII(a) == FoldASCII(b); });
    }
    inline bool TextContains(std::string_view text, std::string_view query, bool sensitive = false)
    {
        if (sensitive) return text.find(query) != std::string_view::npos;
        return query.empty() || std::search(text.begin(), text.end(), query.begin(), query.end(),
            [](unsigned char a, unsigned char b) { return FoldASCII(a) == FoldASCII(b); }) != text.end();
    }

    class PreparedRecordFilters
    {
    public:
        explicit PreparedRecordFilters(std::span<const AdvancedFilterRule> source = {})
        {
            for (const auto& rule : source) {
                if (!rule.enabled || rule.value.empty()) continue;
                Prepared prepared{ rule, {} };
                if (rule.match == AdvancedFilterMatch::Regex) {
                    try { prepared.pattern.emplace(rule.value, std::regex_constants::icase | std::regex_constants::optimize); }
                    catch (const std::regex_error&) { ++invalidPatterns; }
                }
                rules.push_back(std::move(prepared));
            }
        }

        bool Passes(const FormEntry& record) const
        {
            for (const auto& prepared : rules) {
                const auto& rule = prepared.rule;
                if (!rule.targetPlugins.empty() && !std::ranges::any_of(rule.targetPlugins,
                        [&](const auto& plugin) { return TextEquals(record.sourcePlugin, plugin); })) continue;
                const auto matches = [&](std::string_view value) {
                    if (value.empty()) return false;
                    switch (rule.match) {
                    case AdvancedFilterMatch::Contains: return TextContains(value, rule.value);
                    case AdvancedFilterMatch::Exact: return TextEquals(value, rule.value);
                    case AdvancedFilterMatch::Regex: return prepared.pattern && std::regex_search(value.begin(), value.end(), *prepared.pattern);
                    }
                    return false;
                };
                const auto keywordMatches = [&] { return std::ranges::any_of(record.keywords, matches); };
                bool matched{};
                switch (rule.field) {
                case AdvancedFilterField::Any: matched = matches(record.name) || matches(record.editorID) || matches(record.sourcePlugin) || matches(record.category) || keywordMatches(); break;
                case AdvancedFilterField::Name: matched = matches(record.name); break;
                case AdvancedFilterField::EditorID: matched = matches(record.editorID); break;
                case AdvancedFilterField::Plugin: matched = matches(record.sourcePlugin); break;
                case AdvancedFilterField::Category: matched = matches(record.category); break;
                case AdvancedFilterField::Keyword: matched = keywordMatches(); break;
                }
                if (matched) return false;
            }
            return true;
        }
        std::size_t InvalidPatternCount() const { return invalidPatterns; }

    private:
        struct Prepared { AdvancedFilterRule rule; std::optional<std::regex> pattern; };
        std::vector<Prepared> rules;
        std::size_t invalidPatterns{};
    };

    struct CatalogQuery
    {
        std::string type;
        std::vector<std::string> types;
        std::string plugin;
        std::string search;
        bool caseSensitive{};
        bool showPlayable{ true };
        bool showNonPlayable{ true };
        bool showNamed{ true };
        bool showUnnamed{ true };
        bool showDeleted{ true };
        int sortColumn{ 1 };
        bool ascending{ true };
        std::unordered_set<std::string> hiddenPlugins;
        NPCQuery npc;
        bool searchNPCMetadata{ true };
        bool structuredSearch{};
        RecordScope scope;
        bool operator==(const CatalogQuery&) const = default;

        bool Matches(const FormEntry& record, const PreparedRecordFilters& filters, const ParsedSearch* parsed = nullptr) const
        {
            if ((!type.empty() && type != record.category) || (!plugin.empty() && plugin != record.sourcePlugin) ||
                hiddenPlugins.contains(record.sourcePlugin) || (record.isDeleted && !showDeleted) ||
                (record.isPlayable ? !showPlayable : !showNonPlayable) || (record.name.empty() ? !showUnnamed : !showNamed)) return false;
            if (!types.empty() && std::ranges::find(types, record.category) == types.end()) return false;
            if (scope.kind == SearchScope::SelectedPlugins && !std::ranges::any_of(scope.plugins, [&](const auto& name) { return SearchIdentityEquals(name, record.sourcePlugin); })) return false;
            if (scope.kind == SearchScope::Collection && std::ranges::find(scope.records, record.formID) == scope.records.end()) return false;
            if (!filters.Passes(record)) return false;
            if (!npc.Matches(record)) return false;
            if (structuredSearch) return MatchesSearch(record, parsed ? *parsed : ParseSearch(search), caseSensitive);
            if (search.empty()) return true;
            char id[9]{};
            std::snprintf(id, sizeof(id), "%08X", record.formID);
            const std::array<std::string_view, 7> values{ record.name, record.sourcePlugin, record.category,
                searchNPCMetadata ? record.race : std::string_view{}, searchNPCMetadata ? record.factions : std::string_view{}, record.editorID, id };
            return std::ranges::any_of(values, [&](auto value) { return SearchContains(value, search, caseSensitive); });
        }
    };

    inline CatalogResult QueryCatalog(std::shared_ptr<const CatalogSnapshot> snapshot, const CatalogQuery& query,
        const PreparedRecordFilters& filters, std::uint64_t revision)
    {
        CatalogResult result{ std::move(snapshot), revision, {} };
        if (!result.snapshot || !result.snapshot->ready) return result;
        const auto parsed = query.structuredSearch ? ParseSearch(query.search) : ParsedSearch{};
        if (!parsed) return result;
        const auto append = [&](RecordIndex index) {
            if (query.Matches(result.snapshot->records[index], filters, &parsed)) result.order.push_back(index);
        };
        if (!query.type.empty()) {
            const auto found = result.snapshot->byType.find(query.type);
            if (found != result.snapshot->byType.end()) for (const auto index : found->second) append(index);
        } else if (!query.types.empty()) {
            std::unordered_set<std::string_view> visited;
            for (const auto& type : query.types) {
                if (!visited.insert(type).second) continue;
                const auto found = result.snapshot->byType.find(type);
                if (found != result.snapshot->byType.end()) for (const auto index : found->second) append(index);
            }
        } else if (!query.plugin.empty()) {
            const auto found = result.snapshot->byPlugin.find(query.plugin);
            if (found != result.snapshot->byPlugin.end()) for (const auto index : found->second) append(index);
        } else {
            for (RecordIndex i = 0; i < result.snapshot->records.size(); ++i) append(i);
        }
        std::ranges::sort(result.order, [&](RecordIndex a, RecordIndex b) {
            const auto& left = result.snapshot->records[a];
            const auto& right = result.snapshot->records[b];
            int order{};
            if (query.sortColumn == 0) order = (left.formID > right.formID) - (left.formID < right.formID);
            else if (RecordColumnNumeric(query.sortColumn)) {
                const auto l = RecordColumnNumber(left, query.sortColumn), r = RecordColumnNumber(right, query.sortColumn);
                if (l.has_value() != r.has_value()) return l.has_value();
                if (l && r) order = (*l > *r) - (*l < *r);
            } else order = RecordColumnText(left, query.sortColumn).compare(RecordColumnText(right, query.sortColumn));
            return order == 0 ? left.formID < right.formID : query.ascending ? order < 0 : order > 0;
        });
        return result;
    }
}
