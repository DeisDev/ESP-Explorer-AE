#pragma once

#include "pch.h"

#include <deque>
#include <fstream>

namespace ESPExplorerAE
{
    class Logger
    {
    public:
        static void Initialize(bool verboseEnabled);
        static void SetVerboseEnabled(bool enabled);
        static bool IsVerboseEnabled();

        static void Verbose(std::string_view message);
        static void Info(std::string_view message);
        static void Warn(std::string_view message);
        static void Error(std::string_view message);

        static std::filesystem::path GetLogDirectory();
        static std::filesystem::path GetMainLogPath();
        static std::vector<std::string> GetRecentLines();

    private:
        static void Log(std::string_view level, std::string_view message, bool verboseOnly);
        static std::string BuildLogLine(std::string_view level, std::string_view message);
        static std::filesystem::path ResolveDocumentsPath();
        static void PushRecentLine(std::string line);

        static inline std::filesystem::path logDirectory{};
        static inline std::filesystem::path mainLogPath{};
        static inline std::ofstream stream{};
        static inline std::mutex mutex{};
        static inline bool initialized{ false };
        static inline bool verbose{ true };
        static inline std::deque<std::string> recentLines{};
        static inline constexpr std::size_t kMaxRecentLines = 4000;
    };
}
