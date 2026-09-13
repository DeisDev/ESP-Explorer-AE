#pragma once

#include "Core/CatalogQuery.h"

namespace ESPExplorerAE
{
    // Session-local return context; never written to the portable workspace file.
    struct InventoryLocation
    {
        std::string search;
        int category{};
        std::array<bool, 4> filters{};
        std::vector<std::uint64_t> selection;
        std::uint64_t active{};
        std::uint64_t detailsGroup{};
        std::uint64_t detailsToken{};
        std::unordered_map<std::uint64_t, std::uint64_t> choices;
        float scroll{};
        float tableWidth{};
        float tableHeight{};
        std::array<RecordColumnLayout, 10> columns{};
        std::vector<std::pair<int, bool>> sort;
        bool hasLayout{};
    };

    struct WorkspaceLocation
    {
        std::string page{ "Plugin Browser" };
        std::string category;
        CatalogQuery query;
        std::vector<std::uint32_t> selection;
        std::uint32_t active{};
        float resultsScroll{};
        float sourceWidth{ 240.0f };
        float inspectorWidth{ 380.0f };
        bool sourcesOpen{ true };
        bool inspectorOpen{ true };
        bool allRuntimeRecords{};
        RecordTableLayout columns{ DefaultRecordColumns() };
        std::optional<InventoryLocation> inventory;
    };

    struct InspectionLocation
    {
        std::uint32_t formID{};
        float scroll{};
        WorkspaceLocation workspace;
    };

    class InspectionHistory
    {
    public:
        static constexpr std::size_t Capacity = 48;
        const InspectionLocation* Current() const { return entries.empty() ? nullptr : &entries[cursor]; }
        InspectionLocation* Current() { return entries.empty() ? nullptr : &entries[cursor]; }
        bool CanBack() const { return !entries.empty() && cursor > 0; }
        bool CanForward() const { return !entries.empty() && cursor + 1 < entries.size(); }
        void Open(std::uint32_t formID, WorkspaceLocation workspace)
        {
            if (!formID || (Current() && Current()->formID == formID)) return;
            if (!entries.empty()) entries.resize(cursor + 1);
            entries.push_back({ formID, 0.0f, std::move(workspace) });
            if (entries.size() > Capacity) entries.erase(entries.begin());
            cursor = entries.size() - 1;
        }
        bool Back() { if (!CanBack()) return false; --cursor; return true; }
        bool Forward() { if (!CanForward()) return false; ++cursor; return true; }
        void Clear() { entries.clear(); cursor = 0; }
    private:
        std::vector<InspectionLocation> entries;
        std::size_t cursor{};
    };
}
