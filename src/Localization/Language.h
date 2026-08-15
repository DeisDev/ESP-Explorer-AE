#pragma once

#include "Core/LanguageSnapshot.h"
#include <filesystem>
#include <functional>

namespace ESPExplorerAE
{
    class Language
    {
    public:
        using Definition = LanguageDefinition;
        using Diagnostic = std::function<void(std::string)>;

        static bool Load(std::string_view languageCode, const Diagnostic& diagnostic = {});
        static bool LoadFromDirectory(const std::filesystem::path& directory, std::string_view languageCode, const Diagnostic& diagnostic = {});
        static std::shared_ptr<const LanguageSnapshot> Read();
        static std::string GetCopy(std::string_view section, std::string_view key);
        static std::vector<Definition> ListAvailableLanguages();
        static std::vector<Definition> ListAvailableLanguages(const std::filesystem::path& directory);
        static std::string GetCurrentLanguageCode();

        // Pins one locale for legacy immediate-mode widgets throughout a frame.
        // FrameText borrows only within this scope; other readers use Read/GetCopy.
        // Nested scopes restore the enclosing frame. Each thread owns its pin.
        class Frame
        {
        public:
            Frame();
            ~Frame();
            Frame(const Frame&) = delete;
            Frame& operator=(const Frame&) = delete;
        private:
            std::shared_ptr<const LanguageSnapshot> snapshot;
            const LanguageSnapshot* previous{};
        };
        static std::string_view FrameText(std::string_view section, std::string_view key);
    };
}
