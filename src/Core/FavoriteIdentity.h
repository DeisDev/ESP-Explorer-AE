#pragma once

#include "Core/CatalogSnapshot.h"

#include <charconv>
#include <cstdio>
#include <string_view>

namespace ESPExplorerAE
{
    struct FavoriteKey
    {
        std::string plugin;
        std::uint32_t localID{};
        bool light{};
        friend bool operator==(const FavoriteKey&, const FavoriteKey&) = default;
    };

    enum class FavoriteResolution { Resolved, Unavailable, InvalidKey, MissingPlugin, AmbiguousPlugin, ChangedPluginKind, MissingRecord, TemporaryRecord };
    struct FavoriteTarget
    {
        FavoriteResolution status{ FavoriteResolution::Unavailable };
        std::uint32_t formID{};
        std::optional<FavoriteKey> key;
        explicit operator bool() const { return status == FavoriteResolution::Resolved; }
    };

    inline std::string CanonicalPluginName(std::string_view value)
    {
        std::string result(value);
        for (auto& ch : result) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        return result;
    }

    inline bool ValidFavoriteKey(const FavoriteKey& key)
    {
        if (key.plugin.empty() || key.plugin.size() > 255 || key.plugin == "." || key.plugin == ".." ||
            key.localID > (key.light ? 0xFFFu : 0xFFFFFFu) || key.plugin.back() == '.' || key.plugin.back() == ' ') return false;
        for (const unsigned char ch : key.plugin) if (ch < 32 || ch == 127 || std::string_view("<>:\"/\\|?*").find(static_cast<char>(ch)) != std::string_view::npos) return false;
        return true;
    }

    // This is the namespace that owns an ID, not GetFile(0), a winning override,
    // or an EditorID/name heuristic. Missing/duplicate namespaces stay unresolved.
    class FavoriteIdentity
    {
    public:
        explicit FavoriteIdentity(const CatalogSnapshot& value) : catalog(value)
        {
            for (const auto& plugin : catalog.plugins) {
                Add(names, CanonicalPluginName(plugin.filename), &plugin);
                if (const auto prefix = Prefix(plugin)) Add(prefixes, *prefix, &plugin);
            }
        }

        FavoriteTarget Capture(std::uint32_t formID) const
        {
            if (!catalog.ready) return {};
            if ((formID >> 24) == 0xFF) return { FavoriteResolution::TemporaryRecord };
            if (!catalog.Find(formID)) return { FavoriteResolution::MissingRecord };
            const bool light = (formID >> 24) == 0xFE;
            const auto prefix = formID & (light ? 0xFFFFF000u : 0xFF000000u);
            const auto found = prefixes.find(prefix);
            if (found == prefixes.end()) return { FavoriteResolution::MissingPlugin };
            if (!found->second) return { FavoriteResolution::AmbiguousPlugin };
            FavoriteKey key{ CanonicalPluginName(found->second->filename), formID & (light ? 0xFFFu : 0xFFFFFFu), light };
            auto result = Resolve(key);
            if (result && result.formID != formID) return { FavoriteResolution::AmbiguousPlugin };
            return result;
        }

        FavoriteTarget Resolve(const FavoriteKey& key) const
        {
            if (!catalog.ready) return {};
            if (!ValidFavoriteKey(key)) return { FavoriteResolution::InvalidKey };
            const auto found = names.find(CanonicalPluginName(key.plugin));
            if (found == names.end()) return { FavoriteResolution::MissingPlugin };
            if (!found->second) return { FavoriteResolution::AmbiguousPlugin };
            const auto& plugin = *found->second;
            if (plugin.isLight != key.light) return { FavoriteResolution::ChangedPluginKind };
            const auto prefix = Prefix(plugin);
            if (!prefix) return { FavoriteResolution::MissingPlugin };
            const auto origin = prefixes.find(*prefix);
            if (origin == prefixes.end() || origin->second != &plugin) return { FavoriteResolution::AmbiguousPlugin };
            const auto formID = *prefix | key.localID;
            if (!catalog.Find(formID)) return { FavoriteResolution::MissingRecord };
            return { FavoriteResolution::Resolved, formID, FavoriteKey{ CanonicalPluginName(key.plugin), key.localID, key.light } };
        }

    private:
        static std::optional<std::uint32_t> Prefix(const PluginInfo& plugin)
        {
            if (plugin.isLight) return plugin.lightOrder <= 0xFFF ? std::optional{ 0xFE000000u | (plugin.lightOrder << 12) } : std::nullopt;
            return plugin.loadOrder < 0xFE ? std::optional{ plugin.loadOrder << 24 } : std::nullopt;
        }
        template <class Map, class Key> static void Add(Map& map, Key key, const PluginInfo* plugin)
        {
            const auto [it, inserted] = map.emplace(std::move(key), plugin);
            if (!inserted) it->second = nullptr;
        }
        const CatalogSnapshot& catalog;
        std::unordered_map<std::string, const PluginInfo*> names;
        std::unordered_map<std::uint32_t, const PluginInfo*> prefixes;
    };

    inline std::string SerializeFavoriteKey(const FavoriteKey& key)
    {
        if (!ValidFavoriteKey(key)) return {};
        constexpr char hex[] = "0123456789ABCDEF";
        std::string result = key.light ? "L|" : "F|";
        for (const unsigned char ch : CanonicalPluginName(key.plugin)) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == '_') result += static_cast<char>(ch);
            else { result += '%'; result += hex[ch >> 4]; result += hex[ch & 15]; }
        }
        char suffix[10]{};
        std::snprintf(suffix, sizeof(suffix), "|%08X", key.localID);
        return result + suffix;
    }

    inline std::optional<FavoriteKey> ParseFavoriteKey(std::string_view value)
    {
        if (value.size() < 12 || value[1] != '|' || (value[0] != 'F' && value[0] != 'L')) return {};
        const auto separator = value.find('|', 2);
        if (separator == std::string_view::npos || value.size() - separator != 9) return {};
        FavoriteKey key{ .light = value[0] == 'L' };
        const auto number = value.substr(separator + 1);
        const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), key.localID, 16);
        if (error != std::errc{} || end != number.data() + number.size()) return {};
        for (std::size_t i = 2; i < separator; ++i) {
            if (value[i] == '%') {
                if (i + 2 >= separator) return {};
                unsigned decoded{};
                const auto [position, status] = std::from_chars(value.data() + i + 1, value.data() + i + 3, decoded, 16);
                if (status != std::errc{} || position != value.data() + i + 3) return {};
                key.plugin += static_cast<char>(decoded);
                i += 2;
            } else key.plugin += value[i];
        }
        key.plugin = CanonicalPluginName(key.plugin);
        return ValidFavoriteKey(key) ? std::optional{ std::move(key) } : std::nullopt;
    }

    // Keep the historical parser's strict full-token hexadecimal behavior. Bad
    // tokens are retained by the document instead of being silently discarded.
    inline std::optional<std::uint32_t> ParseLegacyFavorite(std::string_view token)
    {
        if (token.empty()) return {};
        std::uint32_t value{};
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value, 16);
        return error == std::errc{} && end == token.data() + token.size() ? std::optional{ value } : std::nullopt;
    }
}
