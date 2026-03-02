#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    struct FormEntry
    {
        std::uint32_t formID{ 0 };
        std::string name;
        std::string category;
        std::string sourcePlugin;
        bool isPlayable{ true };
        bool isDeleted{ false };
    };

    struct FormCache
    {
        std::vector<FormEntry> allRecords;
        std::vector<FormEntry> weapons;
        std::vector<FormEntry> armors;
        std::vector<FormEntry> ammo;
        std::vector<FormEntry> misc;
        std::vector<FormEntry> npcs;
        std::vector<FormEntry> activators;
        std::vector<FormEntry> containers;
        std::vector<FormEntry> statics;
        std::vector<FormEntry> furniture;
        std::vector<FormEntry> spells;
        std::vector<FormEntry> perks;
    };

    struct PluginInfo
    {
        std::string filename;
        std::string type;
        std::uint32_t loadOrder;
        std::uint32_t lightOrder;
        bool isLight;
    };

    struct FormCategoryCounts
    {
        std::size_t weapons{ 0 };
        std::size_t armors{ 0 };
        std::size_t ammo{ 0 };
        std::size_t misc{ 0 };
        std::size_t npcs{ 0 };
        std::size_t activators{ 0 };
        std::size_t containers{ 0 };
        std::size_t statics{ 0 };
        std::size_t furniture{ 0 };
        std::size_t spells{ 0 };
        std::size_t perks{ 0 };
    };

    class DataManager
    {
    public:
        struct DataView
        {
            std::shared_lock<std::shared_mutex> lock;
            const std::vector<PluginInfo>* plugins;
            const FormCategoryCounts* counts;
            const FormCache* formCache;
            const std::uint64_t* dataVersion;

            const std::vector<PluginInfo>& GetPlugins() const { return *plugins; }
            const FormCategoryCounts& GetCounts() const { return *counts; }
            const FormCache& GetFormCache() const { return *formCache; }
            std::uint64_t GetDataVersion() const { return *dataVersion; }
        };

        static void Refresh();
        static DataView GetDataView();
        static std::uint64_t GetDataVersion();
        static std::vector<PluginInfo> GetPlugins();
        static FormCategoryCounts GetCounts();
        static FormCache GetFormCache();

    private:
        static bool PassesFilters(RE::TESForm* form, std::string_view name, bool isPlayable);
        static std::string GetFormName(RE::TESForm* form);
        static std::string GetSourcePluginName(RE::TESForm* form);
        static bool IsPlayable(RE::TESForm* form);

        static inline std::vector<PluginInfo> plugins{};
        static inline FormCache formCache{};
        static inline FormCategoryCounts counts{};
        static inline std::uint64_t dataVersion{ 0 };
        static inline std::shared_mutex dataMutex{};
    };
}
