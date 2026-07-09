#pragma once

#include "Core/Logs.h"
#include <deque>
#include <filesystem>
#include <memory>

namespace ESPExplorerAE
{
    // Render-owner service. Filesystem access is bounded per poll; published
    // values own their strings and may outlive later reads or file rotation.
    class LogReader
    {
    public:
        static constexpr std::size_t MaxLines = 4000;
        static constexpr std::size_t MaxBytes = 1024 * 1024;
        static constexpr std::size_t ReadBudget = 512 * 1024;
        static constexpr std::size_t MaxLineBytes = 64 * 1024;
        LogReader(std::filesystem::path directory, std::filesystem::path mainFile);
        void Refresh();
        bool Select(std::string_view name);
        void Poll();
        std::shared_ptr<const LogSnapshot> Read() const { return published; }
        const std::filesystem::path& Directory() const { return directory; }
        std::filesystem::path SelectedPath() const;
    private:
        void Reset();
        void Consume(std::string_view bytes);
        void Publish();
        std::filesystem::path directory, mainFile;
        std::vector<std::filesystem::path> files;
        std::filesystem::path selected;
        std::deque<std::string> lines;
        std::string pending;
        std::size_t retainedBytes{};
        std::uintmax_t offset{};
        std::filesystem::file_time_type writeTime{};
        bool initial{ true }, discardLine{};
        std::uint64_t selectionRevision{};
        std::shared_ptr<const LogSnapshot> published = std::make_shared<const LogSnapshot>();
    };
}
