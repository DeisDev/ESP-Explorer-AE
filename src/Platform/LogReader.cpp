#include "Platform/LogReader.h"
#include <algorithm>
#include <fstream>

namespace ESPExplorerAE
{
    namespace
    {
        std::string Name(const std::filesystem::path& path)
        {
            const auto bytes = path.filename().u8string();
            return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
        }
    }
    LogReader::LogReader(std::filesystem::path directory, std::filesystem::path mainFile) :
        directory(std::move(directory)), mainFile(std::move(mainFile)) {}

    void LogReader::Reset()
    {
        lines.clear(); pending.clear(); retainedBytes = 0; offset = 0;
        writeTime = {}; initial = true; discardLine = false;
        ++selectionRevision;
    }
    void LogReader::Publish()
    {
        auto next = std::make_shared<LogSnapshot>();
        for (const auto& file : files) next->files.push_back(Name(file));
        next->selectedFile = Name(selected);
        next->lines.assign(lines.begin(), lines.end());
        if (!pending.empty() && !discardLine) next->lines.push_back(pending);
        next->selectionRevision = selectionRevision;
        published = std::move(next);
    }
    void LogReader::Refresh()
    {
        std::vector<std::filesystem::path> found;
        std::error_code error;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            if (!it->is_regular_file(error)) continue;
            auto path = it->path();
            const auto extension = path.extension().string();
            if (extension == ".log" || extension == ".txt") found.push_back(std::move(path));
        }
        std::ranges::sort(found, [](const auto& left, const auto& right) { return Name(left) < Name(right); });
        files = std::move(found);
        if (std::ranges::find(files, selected) == files.end()) {
            const auto preferred = std::ranges::find(files, directory / mainFile);
            selected = preferred != files.end() ? *preferred : files.empty() ? std::filesystem::path{} : files.front();
        }
        Reset();
        Publish();
        Poll();
    }
    bool LogReader::Select(std::string_view name)
    {
        const auto match = std::ranges::find_if(files, [&](const auto& file) { return Name(file) == name; });
        if (match == files.end()) return false;
        selected = *match;
        Reset(); Publish(); Poll();
        return true;
    }
    std::filesystem::path LogReader::SelectedPath() const
    {
        return std::ranges::find(files, selected) != files.end() ? selected : std::filesystem::path{};
    }
    void LogReader::Consume(std::string_view bytes)
    {
        for (const char value : bytes) {
            if (value == '\n') {
                if (!discardLine) {
                    if (!pending.empty() && pending.back() == '\r') pending.pop_back();
                    retainedBytes += pending.size();
                    lines.push_back(std::move(pending));
                }
                pending.clear(); discardLine = false;
            } else if (!discardLine) {
                if (pending.size() == MaxLineBytes) {
                    pending.clear(); discardLine = true;
                    ++selectionRevision;
                } else pending.push_back(value);
            }
            // Include an incomplete tail line in the row limit. A shifted index must
            // never silently select a different line in the UI.
            while (lines.size() + (pending.empty() ? 0 : 1) > MaxLines || retainedBytes + pending.size() > MaxBytes) {
                retainedBytes -= lines.front().size(); lines.pop_front();
                ++selectionRevision;
            }
        }
    }
    void LogReader::Poll()
    {
        if (selected.empty()) return;
        std::error_code error;
        const auto size = std::filesystem::file_size(selected, error);
        if (error) { if (!initial || !lines.empty()) { Reset(); Publish(); } return; }
        const auto changed = std::filesystem::last_write_time(selected, error);
        if (error) return;
        if (!initial && (size < offset || (size == offset && changed != writeTime))) Reset();
        if (!initial && size == offset && changed == writeTime) return;
        std::ifstream input(selected, std::ios::binary);
        if (!input) return;
        if (initial && size > ReadBudget) {
            offset = size - ReadBudget;
            input.seekg(static_cast<std::streamoff>(offset - 1));
            discardLine = input.get() != '\n';
        }
        input.seekg(static_cast<std::streamoff>(offset));
        std::string bytes(static_cast<std::size_t>((std::min)(size - offset, std::uintmax_t{ ReadBudget })), '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (input.bad()) return;
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        Consume(bytes);
        offset += bytes.size(); writeTime = changed; initial = false;
        Publish();
    }
}
