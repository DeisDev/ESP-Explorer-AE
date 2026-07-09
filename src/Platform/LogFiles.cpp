#include "Platform/LogFiles.h"
#include "Core/ScopeExit.h"
#include <ShlObj_core.h>
#include <commdlg.h>
#include <shellapi.h>
#include <array>
#include <algorithm>
#include <ctime>
#include <chrono>

namespace ESPExplorerAE::LogFiles
{
    namespace
    {
        std::wstring Wide(std::string_view text)
        {
            const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring result(size, L'\0');
            if (size) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
            return result;
        }
    }
    std::filesystem::path Documents()
    {
        PWSTR path{};
        const auto result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &path);
        ScopeExit release([&] { CoTaskMemFree(path); });
        return SUCCEEDED(result) && path ? std::filesystem::path(path) : std::filesystem::path(L"Documents");
    }
    void Open(const std::filesystem::path& path)
    {
        if (!path.empty()) ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    void Export(HWND owner, const std::filesystem::path& source, std::string_view logLabel, std::string_view allLabel)
    {
        std::error_code error;
        if (source.empty() || !std::filesystem::is_regular_file(source, error)) return;
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm time{}; localtime_s(&time, &now);
        wchar_t timestamp[32]{};
        std::wcsftime(timestamp, std::size(timestamp), L"%Y%m%d-%H%M%S", &time);
        const auto extension = source.extension().wstring();
        const auto suggested = source.parent_path() / (source.stem().wstring() + L"-" + timestamp + extension);
        std::array<wchar_t, 32768> buffer{};
        if (suggested.native().size() >= buffer.size()) return;
        std::ranges::copy(suggested.native(), buffer.begin());
        auto filter = Wide(logLabel) + L" (*.log;*.txt)";
        filter.push_back(L'\0'); filter += L"*.log;*.txt"; filter.push_back(L'\0');
        filter += Wide(allLabel) + L" (*.*)";
        filter.push_back(L'\0'); filter += L"*.*"; filter.push_back(L'\0'); filter.push_back(L'\0');
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog); dialog.hwndOwner = owner;
        dialog.lpstrFile = buffer.data(); dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrFilter = filter.c_str(); dialog.nFilterIndex = 1;
        dialog.lpstrDefExt = extension.empty() ? L"log" : extension.c_str() + 1;
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        bool picked{};
        {
            int calls{};
            ScopeExit restore([&] { for (int i = 0; i < calls; ++i) ShowCursor(FALSE); });
            do { ++calls; } while (ShowCursor(TRUE) < 0 && calls < 32);
            picked = GetSaveFileNameW(&dialog) == TRUE;
        }
        if (!picked) return;
        const std::filesystem::path destination(buffer.data());
        if (std::filesystem::equivalent(source, destination, error)) return;
        error.clear();
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
        if (error) return;
        const auto args = L"/select,\"" + destination.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}
