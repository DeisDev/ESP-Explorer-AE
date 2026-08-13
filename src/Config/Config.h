#pragma once

#include "Core/Settings.h"
#include <filesystem>

namespace ESPExplorerAE
{
    class Config
    {
    public:
        static bool Load();
        static bool Save();
        static void RequestSave();
        static bool FlushPendingSaveIfDue();
        static bool FlushPendingSave();
        static bool HasPendingSave();
        static const Settings& Get();
        static Settings& GetMutable();

    private:
        static inline Settings settings{};
        static inline std::filesystem::path configPath{};
    };
}
