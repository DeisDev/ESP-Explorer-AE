#include "Config/WorkspaceIni.h"
#include <SimpleIni.h>
#include <cmath>
#include <set>

namespace ESPExplorerAE
{
    namespace
    {
        struct Codec
        {
            CSimpleIniA ini;
            bool reading{};
            bool valid{true};
            std::size_t records{};
            std::set<std::string> visited;
            void Text(const std::string& section, const char* key, std::string& value, std::size_t limit = 1024)
            {
                visited.insert(section + "\n" + key);
                constexpr char hex[] = "0123456789ABCDEF";
                if (reading) {
                    const auto* raw = ini.GetValue(section.c_str(), key, nullptr);
                    if (!raw) { valid = false; return; }
                    const std::string_view encoded = raw;
                    if (encoded.size() > limit * 2 || encoded.size() % 2) { valid = false; return; }
                    value.clear();
                    for (std::size_t i = 0; i < encoded.size(); i += 2) {
                        unsigned byte{};
                        const auto [end, error] = std::from_chars(encoded.data() + i, encoded.data() + i + 2, byte, 16);
                        if (error != std::errc{} || end != encoded.data() + i + 2 || byte == 0) { valid = false; return; }
                        value += static_cast<char>(byte);
                    }
                } else {
                    if (value.size() > limit || value.find('\0') != std::string::npos) { valid = false; return; }
                    std::string encoded;
                    for (const unsigned char byte : value) { encoded += hex[byte >> 4]; encoded += hex[byte & 15]; }
                    valid &= ini.SetValue(section.c_str(), key, encoded.c_str()) >= 0;
                }
            }
            template<class T> void Number(const std::string& section, const char* key, T& value, T minimum, T maximum)
            {
                visited.insert(section + "\n" + key);
                if (reading) {
                    const char* text = ini.GetValue(section.c_str(), key, nullptr);
                    if (!text) { valid = false; return; }
                    const auto* end = text + std::char_traits<char>::length(text);
                    const auto [position, error] = std::from_chars(text, end, value);
                    valid &= error == std::errc{} && position == end;
                } else valid &= ini.SetValue(section.c_str(), key, std::to_string(value).c_str()) >= 0;
                valid &= std::isfinite(static_cast<double>(value)) && value >= minimum && value <= maximum;
            }
            void Flag(const std::string& section, const char* key, bool& value)
            {
                int number = value ? 1 : 0; Number(section, key, number, 0, 1); value = number != 0;
            }
            template<class T> void Enum(const std::string& section, const char* key, T& value, int maximum)
            {
                int number = static_cast<int>(value); Number(section, key, number, 0, maximum); value = static_cast<T>(number);
            }
            template<class T, class F> void List(const std::string& section, std::vector<T>& values, std::size_t limit, F&& visit)
            {
                std::size_t count = values.size(); Number(section, "Count", count, std::size_t{0}, limit);
                if (!valid) return;
                if (reading) values.resize(count);
                for (std::size_t i = 0; i < count && valid; ++i) visit(section + "." + std::to_string(i), values[i]);
            }
            void Record(const std::string& section, WorkspaceRecord& record)
            {
                if (++records > WorkspaceDocument::MaxRecords) { valid = false; return; }
                Text(section, "Identity", record.identity); Text(section, "Name", record.name); Text(section, "Note", record.note, 4096);
            }
            void View(const std::string& section, SavedView& view)
            {
                Text(section, "Name", view.name, 128);
                auto& location = view.location; auto& query = location.query;
                Text(section, "Page", location.page, 32); Text(section, "Category", location.category, 32);
                Text(section, "Search", query.search); Flag(section, "Structured", query.structuredSearch);
                Flag(section, "CaseSensitive", query.caseSensitive);
                Number(section, "Sort", query.sortColumn, 0, static_cast<int>(RecordColumn::Count) - 1); Flag(section, "Ascending", query.ascending);
                Enum(section, "Scope", query.scope.kind, 2); Text(section, "Collection", query.scope.collection, 128);
                List(section + ".Plugins", query.scope.plugins, 4096, [&](const auto& s, auto& plugin) { Text(s, "Name", plugin, 255); });
                Enum(section, "NPCMode", query.npc.mode, 7);
                std::string race = query.npc.selectedRace.value_or(""); Text(section, "Race", race); query.npc.selectedRace = race.empty() ? std::nullopt : std::optional(race);
                std::string faction = query.npc.selectedFaction.value_or(""); Text(section, "Faction", faction); query.npc.selectedFaction = faction.empty() ? std::nullopt : std::optional(faction);
                Number(section, "Scroll", location.resultsScroll, 0.0f, 100000000.0f);
                Number(section, "SourcesWidth", location.sourceWidth, 100.0f, 4096.0f); Number(section, "InspectorWidth", location.inspectorWidth, 100.0f, 4096.0f);
                Flag(section, "SourcesOpen", location.sourcesOpen); Flag(section, "InspectorOpen", location.inspectorOpen);
                std::set<int> orders;
                for (std::size_t i = 0; i < location.columns.size(); ++i) {
                    auto& column = location.columns[i]; const auto s = section + ".Column." + std::to_string(i);
                    Number(s, "Width", column.width, 1.0f, 4096.0f); Number(s, "Order", column.order, 0, static_cast<int>(location.columns.size()) - 1); Flag(s, "Visible", column.visible);
                    valid &= orders.insert(column.order).second;
                }
                List(section + ".Clauses", view.clauses, 64, [&](const auto& s, auto& clause) {
                    Enum(s, "Field", clause.field, 9); Text(s, "Value", clause.value); Flag(s, "Exclude", clause.exclude); Record(s + ".Target", clause.target);
                });
                List(section + ".Selection", view.selected, WorkspaceDocument::MaxRecords, [&](const auto& s, auto& record) { Record(s, record); });
                Record(section + ".Active", view.active);
                constexpr std::array pages{"Plugin Browser", "Item Browser", "NPC Browser", "Cell Browser", "Object Browser", "Spells & Perks"};
                valid &= std::ranges::find(pages, location.page) != pages.end();
                valid &= !query.structuredSearch || query.search.empty();
                if (query.structuredSearch) {
                    std::string expression;
                    for (const auto& clause : view.clauses) {
                        if (!expression.empty()) expression += ' ';
                        expression += SearchClauseText(clause.field, clause.field == SearchField::ID ? "00000001" : clause.value, clause.exclude);
                    }
                    valid &= static_cast<bool>(ParseSearch(expression));
                }
            }
            void Document(WorkspaceDocument& document)
            {
                List("Views", document.views, WorkspaceDocument::MaxGroups, [&](const auto& s, auto& view) { View(s, view); });
                List("Layouts", document.layouts, WorkspaceDocument::MaxGroups, [&](const auto& s, auto& view) { View(s, view); });
                List("Collections", document.collections, WorkspaceDocument::MaxGroups, [&](const auto& s, auto& collection) {
                    Text(s, "Name", collection.name, 128);
                    List(s + ".Records", collection.records, WorkspaceDocument::MaxRecords, [&](const auto& r, auto& record) { Record(r, record); });
                });
                List("Kits", document.kits, WorkspaceDocument::MaxGroups, [&](const auto& s, auto& kit) {
                    Text(s, "Name", kit.name, 128);
                    List(s + ".Entries", kit.entries, 256, [&](const auto& r, auto& entry) {
                        Record(r, entry.record); Number(r, "Quantity", entry.quantity, 1u, 1000000u); Flag(r, "Ammo", entry.includeAmmo); Number(r, "AmmoQuantity", entry.ammoQuantity, 0u, 1000000u);
                    });
                });
                const auto names = [&](const auto& groups) {
                    std::set<std::string> unique;
                    for (const auto& group : groups) valid &= !group.name.empty() && unique.insert(FoldSearchText(group.name)).second;
                };
                names(document.views); names(document.collections); names(document.kits);
                if (reading) {
                    CSimpleIniA::TNamesDepend sections; ini.GetAllSections(sections);
                    for (const auto& section : sections) {
                        CSimpleIniA::TNamesDepend keys; ini.GetAllKeys(section.pItem, keys);
                        for (const auto& key : keys) {
                            CSimpleIniA::TNamesDepend values; ini.GetAllValues(section.pItem, key.pItem, values);
                            valid &= values.size() == 1 && visited.contains(std::string(section.pItem) + "\n" + key.pItem);
                        }
                    }
                }
            }
        };
    }
    WorkspaceReadResult ReadWorkspaceIni(std::string_view bytes)
    {
        WorkspaceReadResult result;
        if (bytes.empty() || bytes.size() > WorkspaceDocument::MaxBytes || bytes.find('\0') != std::string_view::npos) { result.error = "Invalid workspace size or encoding"; return result; }
        Codec codec; codec.reading = true; codec.ini.SetUnicode(); codec.ini.SetMultiKey();
        if (codec.ini.LoadData(bytes.data(), bytes.size()) < 0) { result.error = "Workspace INI parse failed"; return result; }
        int schema{}; codec.Number("Workspace", "Schema", schema, 1, 1000000);
        if (!codec.valid || schema != 1) { result.newerSchema = schema > 1; result.error = "Unsupported workspace schema"; return result; }
        codec.Document(result.document);
        if (!codec.valid) { result.document = {}; result.error = "Invalid or unsupported workspace entries"; }
        return result;
    }
    bool WriteWorkspaceIni(const WorkspaceDocument& document, std::string& bytes)
    {
        Codec codec; codec.ini.SetUnicode(); int schema = 1; codec.Number("Workspace", "Schema", schema, 1, 1);
        auto copy = document; codec.Document(copy);
        bytes.clear();
        return codec.valid && codec.ini.Save(bytes) >= 0 && bytes.size() <= WorkspaceDocument::MaxBytes;
    }
}
