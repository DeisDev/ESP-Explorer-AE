#pragma once

#include "Core/FavoriteIdentity.h"
#include "Core/WorkspaceNavigation.h"
#include "Core/WorkspaceDestination.h"

namespace ESPExplorerAE
{
    struct WorkspaceRecord
    {
        std::string identity;
        std::string name;
        std::string note;
        // Only a live session may use these values. The storage codec omits them.
        std::uint32_t transientID{};
        std::uint64_t session{};
        bool operator==(const WorkspaceRecord&) const = default;
    };
    inline WorkspaceRecord CaptureWorkspaceRecord(const CatalogSnapshot& catalog, std::uint32_t id, std::uint64_t session)
    {
        const auto target = FavoriteIdentity(catalog).Capture(id);
        const auto* record = catalog.Find(id);
        return {target.key ? SerializeFavoriteKey(*target.key) : "", record ? record->name : "", "", target ? 0 : id, session};
    }
    inline FavoriteTarget ResolveWorkspaceRecord(const FavoriteIdentity& identity, const WorkspaceRecord& record, std::uint64_t session)
    {
        if (record.identity.empty()) return record.transientID && record.session == session ?
            FavoriteTarget{FavoriteResolution::Resolved, record.transientID} : FavoriteTarget{FavoriteResolution::TemporaryRecord};
        const auto key = ParseFavoriteKey(record.identity);
        return key ? identity.Resolve(*key) : FavoriteTarget{FavoriteResolution::InvalidKey};
    }
    struct WorkspaceClause
    {
        SearchField field{};
        std::string value;
        bool exclude{};
        WorkspaceRecord target;
    };
    struct SavedView
    {
        std::string name;
        WorkspaceLocation location;
        std::vector<WorkspaceClause> clauses;
        std::vector<WorkspaceRecord> selected;
        WorkspaceRecord active;
    };
    struct RecordCollection
    {
        std::string name;
        std::vector<WorkspaceRecord> records;
    };
    struct KitEntry
    {
        WorkspaceRecord record;
        std::uint32_t quantity{1};
        bool includeAmmo{};
        std::uint32_t ammoQuantity{100};
    };
    struct ItemKit
    {
        std::string name;
        std::vector<KitEntry> entries;
    };
    struct WorkspaceDocument
    {
        static constexpr std::size_t MaxBytes = 2 * 1024 * 1024;
        static constexpr std::size_t MaxGroups = 128;
        static constexpr std::size_t MaxRecords = 4096;
        std::vector<SavedView> views;
        std::vector<RecordCollection> collections;
        std::vector<ItemKit> kits;
        std::vector<SavedView> layouts;
    };
    inline std::string SearchClauseText(SearchField field, std::string_view value, bool exclude)
    {
        constexpr std::array names{"", "name:", "editorid:", "id:", "plugin:", "type:", "keyword:", "damage:", "weight:", "value:"};
        std::string result = exclude ? "-" : "";
        result += names.at(static_cast<std::size_t>(field));
        result += '"';
        for (char ch : value) { if (ch == '"' || ch == '\\') result += '\\'; result += ch; }
        return result + '"';
    }
    inline SavedView CaptureSavedView(std::string name, WorkspaceLocation location, const CatalogSnapshot& catalog, std::uint64_t session)
    {
        SavedView saved{std::move(name), std::move(location)};
        if (saved.location.query.structuredSearch) {
            const auto parsed = ParseSearch(saved.location.query.search);
            if (parsed) {
                for (const auto& clause : parsed.clauses) saved.clauses.push_back({clause.field,
                    clause.field == SearchField::ID ? "" : clause.value, clause.exclude,
                    clause.field == SearchField::ID ? CaptureWorkspaceRecord(catalog, clause.id, session) : WorkspaceRecord{}});
                saved.location.query.search.clear();
            }
        }
        for (auto id : saved.location.selection) saved.selected.push_back(CaptureWorkspaceRecord(catalog, id, session));
        saved.active = CaptureWorkspaceRecord(catalog, saved.location.active, session);
        saved.location.selection.clear();
        saved.location.active = 0;
        saved.location.query.scope.records.clear();
        return saved;
    }
    inline WorkspaceLocation ResolveSavedView(const SavedView& saved, const CatalogSnapshot& catalog, std::uint64_t session)
    {
        auto location = saved.location;
        FavoriteIdentity identity(catalog);
        if (location.query.structuredSearch && location.query.search.empty()) {
            for (const auto& clause : saved.clauses) {
                std::string value = clause.value;
                if (clause.field == SearchField::ID) {
                    const auto target = ResolveWorkspaceRecord(identity, clause.target, session);
                    char id[9]{};
                    // Missing targets produce a visible invalid query instead of
                    // accidentally broadening a saved negative ID predicate.
                    std::snprintf(id, sizeof(id), "%08X", target ? target.formID : 0);
                    value = id;
                }
                if (!location.query.search.empty()) location.query.search += ' ';
                location.query.search += SearchClauseText(clause.field, value, clause.exclude);
            }
        }
        for (const auto& record : saved.selected) if (const auto target = ResolveWorkspaceRecord(identity, record, session)) location.selection.push_back(target.formID);
        if (const auto target = ResolveWorkspaceRecord(identity, saved.active, session)) location.active = target.formID;
        return location;
    }
    inline void ResolveCollectionScope(RecordScope& scope, const WorkspaceDocument& workspace, const CatalogSnapshot& catalog, std::uint64_t session)
    {
        if (scope.kind != SearchScope::Collection) return;
        scope.records.clear();
        FavoriteIdentity identity(catalog);
        for (const auto& collection : workspace.collections) if (collection.name == scope.collection)
            for (const auto& record : collection.records) if (const auto target = ResolveWorkspaceRecord(identity, record, session)) scope.records.push_back(target.formID);
    }
}
