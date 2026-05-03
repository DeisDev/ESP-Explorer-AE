#include "Logging/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <ShlObj_core.h>

namespace ESPExplorerAE
{
    void Logger::Initialize(bool verboseEnabled)
    {
        {
            std::scoped_lock lock(mutex);

            if (initialized) {
                verbose = verboseEnabled;
                return;
            }

            verbose = verboseEnabled;

            logDirectory = ResolveDocumentsPath() / "My Games" / "Fallout4" / "F4SE";
            std::filesystem::create_directories(logDirectory);

            mainLogPath = logDirectory / "ESPExplorerAE.log";
            stream.open(mainLogPath, std::ios::out | std::ios::app);

            initialized = true;
        }

        Log("INFO", "Logger initialized", false);
    }

    void Logger::SetVerboseEnabled(bool enabled)
    {
        {
            std::scoped_lock lock(mutex);
            verbose = enabled;
        }
        Log("INFO", enabled ? "Verbose logging enabled" : "Verbose logging disabled", false);
    }

    bool Logger::IsVerboseEnabled()
    {
        std::scoped_lock lock(mutex);
        return verbose;
    }

    void Logger::Verbose(std::string_view message)
    {
        Log("VERBOSE", message, true);
    }

    void Logger::Info(std::string_view message)
    {
        Log("INFO", message, false);
    }

    void Logger::Warn(std::string_view message)
    {
        Log("WARN", message, false);
    }

    void Logger::Error(std::string_view message)
    {
        Log("ERROR", message, false);
    }

    std::filesystem::path Logger::GetLogDirectory()
    {
        std::scoped_lock lock(mutex);
        if (!initialized) {
            return ResolveDocumentsPath() / "My Games" / "Fallout4" / "F4SE";
        }
        return logDirectory;
    }

    std::filesystem::path Logger::GetMainLogPath()
    {
        std::scoped_lock lock(mutex);
        if (!initialized) {
            return ResolveDocumentsPath() / "My Games" / "Fallout4" / "F4SE" / "ESPExplorerAE.log";
        }
        return mainLogPath;
    }

    std::vector<std::string> Logger::GetRecentLines()
    {
        std::scoped_lock lock(mutex);
        return { recentLines.begin(), recentLines.end() };
    }

    void Logger::Log(std::string_view level, std::string_view message, bool verboseOnly)
    {
        std::scoped_lock lock(mutex);

        if (!initialized) {
            return;
        }

        if (verboseOnly && !verbose) {
            return;
        }

        const auto line = BuildLogLine(level, message);
        PushRecentLine(line);

        if (stream.is_open()) {
            stream << line << '\n';
            stream.flush();
        }
    }

    std::string Logger::BuildLogLine(std::string_view level, std::string_view message)
    {
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);

        std::tm localTime{};
        localtime_s(&localTime, &time);

        std::ostringstream output;
        output << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " [" << level << "] " << message;
        return output.str();
    }

    std::filesystem::path Logger::ResolveDocumentsPath()
    {
        PWSTR path{ nullptr };
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &path);

        if (SUCCEEDED(result) && path) {
            std::filesystem::path docs(path);
            CoTaskMemFree(path);
            return docs;
        }

        if (path) {
            CoTaskMemFree(path);
        }

        return std::filesystem::path("Documents");
    }

    void Logger::PushRecentLine(std::string line)
    {
        recentLines.push_back(std::move(line));
        while (recentLines.size() > kMaxRecentLines) {
            recentLines.pop_front();
        }
    }
}
