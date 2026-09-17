#include "App/WorkspaceService.h"
#include "Config/WorkspaceIni.h"
#include "Core/SaveSchedule.h"
#include "Platform/AtomicFile.h"
#include <fstream>
#include <mutex>

namespace ESPExplorerAE
{
    namespace
    {
        std::mutex workspaceMutex;
        std::shared_ptr<const WorkspaceDocument> workspace = std::make_shared<WorkspaceDocument>();
        SaveSchedule schedule;
        bool writable{};
        std::string pendingBytes;
        std::string storageError;
        const std::filesystem::path workspacePath{"Data/F4SE/Plugins/ESPExplorerAE.workspace.ini"};
    }
    void WorkspaceService::Load()
    {
        std::lock_guard lock(workspaceMutex);
        writable = false;
        workspace = std::make_shared<WorkspaceDocument>();
        schedule = {};
        pendingBytes.clear();
        storageError.clear();
        std::error_code error;
        if (!std::filesystem::exists(workspacePath, error) && !error) { writable = true; return; }
        const auto size = std::filesystem::file_size(workspacePath, error);
        if (error || size > WorkspaceDocument::MaxBytes) { storageError = "Cannot read workspace file"; return; }
        std::ifstream file(workspacePath, std::ios::binary);
        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) { storageError = "Cannot read workspace file"; return; }
        auto parsed = ReadWorkspaceIni(bytes);
        if (!parsed) { storageError = parsed.error; return; }
        workspace = std::make_shared<WorkspaceDocument>(std::move(parsed.document));
        writable = true;
        storageError.clear();
    }
    std::shared_ptr<const WorkspaceDocument> WorkspaceService::Read() { std::lock_guard lock(workspaceMutex); return workspace; }
    bool WorkspaceService::Commit(WorkspaceDocument document)
    {
        std::string bytes;
        if (!WriteWorkspaceIni(document, bytes)) return false;
        std::lock_guard lock(workspaceMutex);
        if (!writable) return false;
        workspace = std::make_shared<WorkspaceDocument>(std::move(document));
        if (bytes == pendingBytes) return true;
        pendingBytes = std::move(bytes);
        schedule.Request(SaveSchedule::Clock::now());
        return true;
    }
    bool WorkspaceService::Flush(bool force)
    {
        std::lock_guard lock(workspaceMutex);
        if (!writable || !(force ? schedule.IsDirty() : schedule.IsDue(SaveSchedule::Clock::now()))) return false;
        const bool saved = WriteFileAtomically(workspacePath, pendingBytes, storageError);
        schedule.Complete(saved, SaveSchedule::Clock::now());
        if (saved) storageError.clear();
        return saved;
    }
    bool WorkspaceService::HasPendingSave() { std::lock_guard lock(workspaceMutex); return schedule.IsDirty(); }
    bool WorkspaceService::Writable() { std::lock_guard lock(workspaceMutex); return writable; }
    std::string WorkspaceService::Error() { std::lock_guard lock(workspaceMutex); return storageError; }
}
