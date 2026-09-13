#include "Config/Config.h"
#include "Config/SettingsIni.h"
#include "Core/SaveSchedule.h"
#include "Core/MigrationWrite.h"
#include "Platform/AtomicFile.h"
#include "App/WorkspaceService.h"

#include <mutex>
#include <fstream>

namespace ESPExplorerAE
{
    namespace
    {
        Settings pendingSettings{};
        std::filesystem::path pendingConfigPath{};
        SaveSchedule saveSchedule;
        std::mutex persistenceMutex;

        std::optional<std::string> legacyBackup;
        bool persistenceAllowed{};

    }

    static std::filesystem::path ResolveConfigPath()
    {
        return std::filesystem::path("Data/F4SE/Plugins/ESPExplorerAE.ini");
    }

    static bool SaveSnapshot(const Settings& settingsSnapshot, const std::filesystem::path& path)
    try {
        std::string serialized;
        if (!WriteSettingsIni(settingsSnapshot, serialized)) {
            REX::WARN("Failed to serialize config {}", path.string());
            return false;
        }
        std::string error;
        return WriteWithMigrationBackup(legacyBackup, [&](const std::string& original) {
            std::filesystem::path backup;
            if (!WriteConfigBackup(path, original, backup, error)) {
                REX::WARN("Cannot back up legacy config {}: {}. Config remains unchanged.", path.string(), error);
                return false;
            }
            REX::INFO("Legacy config preserved at {}", backup.string());
            return true;
        }, [&] {
            if (!WriteFileAtomically(path, serialized, error)) {
                REX::WARN("Cannot save config {}: {}. Changes retained for retry.", path.string(), error);
                return false;
            }
            return true;
        });
    }

    catch (const std::exception& error) {
        REX::WARN("Cannot save config: {}. Changes retained for retry.", error.what());
        return false;
    }

    bool Config::Load()
    {
        WorkspaceService::Load();
        persistenceAllowed = false;
        legacyBackup.reset();
        configPath = ResolveConfigPath();

        std::error_code fileError;
        const bool exists = std::filesystem::exists(configPath, fileError);
        if (fileError) {
            REX::WARN("Cannot read config {}: {}", configPath.string(), fileError.message());
            return false;
        }
        if (!exists) {
            persistenceAllowed = true;
            return Save();
        }

        std::ifstream file(configPath, std::ios::binary);
        if (!file) return false;
        std::string original((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (file.bad()) return false;
        auto parsed = ReadSettingsIni(original);
        if (!parsed) {
            REX::WARN("Cannot load config {}: {}. Configuration was not changed.", configPath.string(), parsed.error);
            return false;
        }
        if (parsed.legacyFavorites) legacyBackup = std::move(original);
        for (const auto& key : parsed.adjusted) REX::WARN("Adjusted invalid or legacy config setting: {}", key);
        settings = std::move(parsed.settings);
        persistenceAllowed = true;
        return true;
    }

    bool Config::Save()
    {
        RequestSave();
        return FlushPendingSave();
    }

    void Config::RequestSave()
    {
        std::lock_guard lock(persistenceMutex);
        if (!persistenceAllowed) return;
        if (configPath.empty()) {
            configPath = ResolveConfigPath();
        }
        pendingSettings = settings;
        pendingConfigPath = configPath;
        saveSchedule.Request(SaveSchedule::Clock::now());
    }

    bool Config::FlushPendingSaveIfDue()
    {
        WorkspaceService::Flush();
        std::lock_guard lock(persistenceMutex);
        if (!persistenceAllowed) return false;
        if (!saveSchedule.IsDue(SaveSchedule::Clock::now())) {
            return false;
        }
        const bool saved = SaveSnapshot(pendingSettings, pendingConfigPath);
        saveSchedule.Complete(saved, SaveSchedule::Clock::now());
        return saved;
    }

    bool Config::FlushPendingSave()
    {
        WorkspaceService::Flush(true);
        std::lock_guard lock(persistenceMutex);
        if (!persistenceAllowed) return false;
        if (!saveSchedule.IsDirty()) {
            return false;
        }
        const bool saved = SaveSnapshot(pendingSettings, pendingConfigPath);
        saveSchedule.Complete(saved, SaveSchedule::Clock::now());
        return saved;
    }

    bool Config::HasPendingSave()
    {
        std::lock_guard lock(persistenceMutex);
        return saveSchedule.IsDirty();
    }

    const Settings& Config::Get()
    {
        return settings;
    }

    Settings& Config::GetMutable()
    {
        return settings;
    }
}
