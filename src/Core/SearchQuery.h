#pragma once

#include "Core/RecordColumns.h"
#include "Core/UnicodeSearch.h"

#include <charconv>
#include <cmath>
#include <vector>

namespace ESPExplorerAE
{
    enum class SearchScope { AllPlugins, SelectedPlugins, Collection };
    struct RecordScope
    {
        SearchScope kind{ SearchScope::AllPlugins };
        std::vector<std::string> plugins;
        std::string collection;
        std::vector<std::uint32_t> records;
        bool operator==(const RecordScope&) const = default;
    };
    enum class SearchField { Any, Name, EditorID, ID, Plugin, Type, Keyword, Damage, Weight, Value };
    enum class SearchError { None, TooLong, TooManyClauses, Quote, Field, EmptyValue, InvalidID, InvalidNumber };
    struct SearchClause
    {
        SearchField field{};
        std::string value;
        bool exclude{};
        std::uint32_t id{};
        double number{};
        std::string operation;
    };
    struct ParsedSearch
    {
        std::vector<SearchClause> clauses;
        SearchError error{};
        explicit operator bool() const { return error == SearchError::None; }
    };
    inline std::optional<std::uint32_t> ParseExactFormID(std::string_view value)
    {
        if (value.size() != 8) return {};
        std::uint32_t id{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), id, 16);
        return error == std::errc{} && end == value.data() + value.size() && id ? std::optional(id) : std::nullopt;
    }
    inline bool SearchIdentityEquals(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size()) return false;
        const auto fold = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        for (std::size_t index = 0; index < a.size(); ++index) if (fold(a[index]) != fold(b[index])) return false;
        return true;
    }
    inline ParsedSearch ParseSearch(std::string_view text)
    {
        ParsedSearch result;
        if (text.size() > 1024) return {{}, SearchError::TooLong};
        const auto space = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };
        for (std::size_t index = 0; index < text.size();) {
            if (space(text[index])) { ++index; continue; }
            if (result.clauses.size() >= 64) return {{}, SearchError::TooManyClauses};
            SearchClause clause;
            if (text[index] == '-') { clause.exclude = true; ++index; }
            std::string token;
            bool quoted{};
            while (index < text.size() && (quoted || !space(text[index]))) {
                const char ch = text[index++];
                if (ch == '"') quoted = !quoted;
                else if (ch == '\\' && quoted) {
                    if (index >= text.size() || (text[index] != '"' && text[index] != '\\')) return {{}, SearchError::Quote};
                    token += text[index++];
                } else token += ch;
            }
            if (quoted) return {{}, SearchError::Quote};
            const auto colon = token.find(':');
            if (colon != std::string::npos) {
                const auto field = token.substr(0, colon);
                if (field == "name") clause.field = SearchField::Name;
                else if (field == "editorid") clause.field = SearchField::EditorID;
                else if (field == "id") clause.field = SearchField::ID;
                else if (field == "plugin") clause.field = SearchField::Plugin;
                else if (field == "type") clause.field = SearchField::Type;
                else if (field == "keyword") clause.field = SearchField::Keyword;
                else if (field == "damage") clause.field = SearchField::Damage;
                else if (field == "weight") clause.field = SearchField::Weight;
                else if (field == "value") clause.field = SearchField::Value;
                else return {{}, SearchError::Field};
                clause.value = token.substr(colon + 1);
            } else clause.value = std::move(token);
            if (clause.value.empty()) return {{}, SearchError::EmptyValue};
            if (clause.field == SearchField::ID) {
                const auto id = ParseExactFormID(clause.value);
                if (!id) return {{}, SearchError::InvalidID};
                clause.id = *id;
            }
            if (clause.field >= SearchField::Damage) {
                const auto first = clause.value.find_first_not_of("<>=");
                if (first == std::string::npos) return {{}, SearchError::InvalidNumber};
                clause.operation = clause.value.substr(0, first);
                if (!clause.operation.empty() && clause.operation != "=" && clause.operation != "<" && clause.operation != ">" && clause.operation != "<=" && clause.operation != ">=") return {{}, SearchError::InvalidNumber};
                const auto number = std::string_view(clause.value).substr(first);
                const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), clause.number);
                if (error != std::errc{} || end != number.data() + number.size() || !std::isfinite(clause.number)) return {{}, SearchError::InvalidNumber};
            }
            result.clauses.push_back(std::move(clause));
        }
        return result;
    }
    inline bool MatchesSearch(const FormEntry& record, const ParsedSearch& query, bool sensitive = false)
    {
        if (!query) return false;
        bool hasPlugin{}, pluginMatch{}, hasType{}, typeMatch{};
        for (const auto& clause : query.clauses) {
            bool matches{};
            switch (clause.field) {
            case SearchField::Any: matches = SearchContains(record.name, clause.value, sensitive) || SearchContains(record.editorID, clause.value, sensitive) || SearchContains(record.sourcePlugin, clause.value, sensitive); break;
            case SearchField::Name: matches = SearchContains(record.name, clause.value, sensitive); break;
            case SearchField::EditorID: matches = SearchContains(record.editorID, clause.value, sensitive); break;
            case SearchField::ID: matches = record.formID == clause.id; break;
            case SearchField::Plugin: matches = SearchIdentityEquals(record.sourcePlugin, clause.value); break;
            case SearchField::Type: matches = SearchIdentityEquals(record.category, clause.value); break;
            case SearchField::Keyword: matches = std::ranges::any_of(record.keywords, [&](const auto& keyword) { return SearchIdentityEquals(keyword, clause.value); }); break;
            default: {
                const auto number = RecordColumnNumber(record, clause.field == SearchField::Damage ? 6 : clause.field == SearchField::Weight ? 7 : 8);
                if (!number) return false;
                matches = clause.operation == "<" ? *number < clause.number : clause.operation == ">" ? *number > clause.number :
                    clause.operation == "<=" ? *number <= clause.number : clause.operation == ">=" ? *number >= clause.number : *number == clause.number;
                break;
            }
            }
            if (clause.exclude) { if (matches) return false; }
            else if (clause.field == SearchField::Plugin) { hasPlugin = true; pluginMatch |= matches; }
            else if (clause.field == SearchField::Type) { hasType = true; typeMatch |= matches; }
            else if (!matches) return false;
        }
        return (!hasPlugin || pluginMatch) && (!hasType || typeMatch);
    }
}
