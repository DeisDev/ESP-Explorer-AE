#include "Platform/AtomicFile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <atomic>
#include <limits>

namespace ESPExplorerAE
{
    static bool WriteAtomic(const std::filesystem::path& path, std::string_view bytes, bool replace, std::string& error) noexcept
    {
        HANDLE file = INVALID_HANDLE_VALUE;
        std::filesystem::path temporary;
        bool ownsTemporary = false;
        try {
            std::error_code ec;
            if (!path.parent_path().empty()) {
                std::filesystem::create_directories(path.parent_path(), ec);
            }
            if (ec) {
                error = "Create config directory: " + ec.message();
                return false;
            }
            static std::atomic<std::uint64_t> sequence{};
            temporary = path;
            temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence);
            file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) {
                error = "Create temporary config: Win32 " + std::to_string(GetLastError());
                return false;
            }
            ownsTemporary = true;
            bool written = bytes.size() <= (std::numeric_limits<DWORD>::max)();
            DWORD count = 0;
            written = written && WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) && count == bytes.size();
            written = written && FlushFileBuffers(file);
            const DWORD writeError = written ? ERROR_SUCCESS : GetLastError();
            const bool closed = CloseHandle(file) != FALSE;
            file = INVALID_HANDLE_VALUE;
            if (!written || !closed) {
                error = "Write/flush temporary config: Win32 " + std::to_string(writeError);
                DeleteFileW(temporary.c_str());
                return false;
            }
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0))) {
                error = "Replace config: Win32 " + std::to_string(GetLastError());
                DeleteFileW(temporary.c_str());
                return false;
            }
            return true;
        } catch (const std::exception& exception) {
            if (file != INVALID_HANDLE_VALUE) {
                CloseHandle(file);
            }
            if (ownsTemporary) {
                DeleteFileW(temporary.c_str());
            }
            try { error = exception.what(); } catch (...) {}
            return false;
        }
    }

    bool WriteFileAtomically(const std::filesystem::path& path, std::string_view bytes, std::string& error) noexcept
    {
        return WriteAtomic(path, bytes, true, error);
    }

    bool WriteConfigBackup(const std::filesystem::path& original, std::string_view bytes, std::filesystem::path& backup, std::string& error) noexcept
    try {
        FILETIME now{};
        GetSystemTimeAsFileTime(&now);
        const auto timestamp = (static_cast<std::uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
        static std::atomic<std::uint64_t> sequence{};
        auto destination = original;
        destination += L".pre-favorites-v1." + std::to_wstring(timestamp) + L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence) + L".bak";
        if (!WriteAtomic(destination, bytes, false, error)) return false;
        backup = std::move(destination);
        return true;
    } catch (const std::exception& exception) {
        try { error = exception.what(); } catch (...) {}
        return false;
    }
}
