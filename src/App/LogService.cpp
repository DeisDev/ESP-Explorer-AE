#include "pch.h"
#include "App/LogService.h"
#include "Core/ScopeExit.h"
#include "Hooks/Hooks.h"
#include "Platform/LogFiles.h"
#include "Platform/LogReader.h"
#include <chrono>

namespace ESPExplorerAE
{
    namespace
    {
        std::unique_ptr<LogReader> reader;
        std::chrono::steady_clock::time_point nextPoll;
        const auto empty = std::make_shared<const LogSnapshot>();
    }
    std::shared_ptr<const LogSnapshot> LogService::Read()
    {
        try {
            if (!reader) {
                auto value = std::make_unique<LogReader>(LogFiles::Documents() / "My Games" / std::string(F4SE::GetSaveFolderName()) / "F4SE",
                    std::string(F4SE::GetPluginName()) + ".log");
                value->Refresh();
                reader = std::move(value);
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= nextPoll) { nextPoll = now + std::chrono::milliseconds(350); reader->Poll(); }
        } catch (const std::exception& error) { REX::WARN("Log viewer read failed: {}", error.what()); }
        return reader ? reader->Read() : empty;
    }
    void LogService::Apply(const LogRequests& requests, std::string_view logLabel, std::string_view allLabel)
    {
        if (!reader) return;
        try {
            if (requests.selectFile) reader->Select(*requests.selectFile);
            if (requests.refresh) reader->Refresh();
            if (requests.openFile) LogFiles::Open(reader->SelectedPath());
            if (requests.openFolder) LogFiles::Open(reader->Directory());
            if (requests.exportFile) {
                Hooks::SetModalDialogActive(true);
                ScopeExit restore([] { Hooks::SetModalDialogActive(false); });
                LogFiles::Export(Hooks::GetGameWindow(), reader->SelectedPath(), logLabel, allLabel);
            }
        } catch (const std::exception& error) { REX::WARN("Log viewer request failed: {}", error.what()); }
    }
    void LogService::Reset() { reader.reset(); nextPoll = {}; }
}
