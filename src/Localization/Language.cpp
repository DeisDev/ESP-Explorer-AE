#include "Core/Profiling.h"
#include "Localization/Language.h"


#include "Core/SnapshotStore.h"
#include "Core/SettingsValidation.h"
#include <SimpleIni.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace ESPExplorerAE
{
    namespace
    {
        SnapshotStore<LanguageSnapshot> snapshots;
        std::mutex reloadMutex;
        thread_local const LanguageSnapshot* frameSnapshot{};

        constexpr std::string_view kLanguageMetaSection = "Language";
        constexpr std::string_view kLanguageNameKey = "sName";
        constexpr std::string_view kLanguageFontsKey = "sFontFiles";
        constexpr std::string_view kLanguageRangesKey = "sGlyphRanges";

        std::filesystem::path ResolveLanguageDirectory()
        {
            const auto runtimePath = std::filesystem::path("Data/Interface/ESPExplorerAE/lang");
            std::error_code error;
            if (std::filesystem::exists(runtimePath, error) || error) {
                return runtimePath;
            }

            return std::filesystem::path("dist/lang");
        }

        std::filesystem::path LanguagePath(const std::filesystem::path& directory, std::string_view code)
        {
            const auto filename = std::string(code) + ".ini";
            return directory / std::filesystem::path(std::u8string(filename.begin(), filename.end()));
        }

        std::string Stem(const std::filesystem::path& path)
        {
            const auto utf8 = path.stem().u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        std::string Trim(std::string_view value)
        {
            std::size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
                ++start;
            }

            std::size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
                --end;
            }

            return std::string(value.substr(start, end - start));
        }

        std::vector<std::string> ParseCsv(std::string_view value)
        {
            std::vector<std::string> result;
            std::size_t start = 0;
            while (start <= value.size()) {
                const auto end = value.find(',', start);
                const auto token = Trim(value.substr(start, end == std::string_view::npos ? value.size() - start : end - start));
                if (!token.empty()) {
                    result.push_back(token);
                }

                if (end == std::string_view::npos) {
                    break;
                }

                start = end + 1;
            }

            return result;
        }

        bool LoadLanguageFile(const std::filesystem::path& path, std::unordered_map<std::string, std::string>& out, Language::Definition* definition)
        {
            CSimpleIniA ini;
            ini.SetUnicode();

            if (definition) {
                definition->code = Stem(path);
                definition->displayName = definition->code;
                definition->fontFiles.clear();
                definition->glyphRanges.clear();
            }

            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (file.bad() || bytes.empty() || bytes.find('\0') != std::string::npos || ini.LoadData(bytes) < 0) return false;

            if (definition) {
                const char* displayName = ini.GetValue(kLanguageMetaSection.data(), kLanguageNameKey.data(), "");
                const char* fontFiles = ini.GetValue(kLanguageMetaSection.data(), kLanguageFontsKey.data(), "");
                const char* glyphRanges = ini.GetValue(kLanguageMetaSection.data(), kLanguageRangesKey.data(), "");

                if (displayName && displayName[0] != '\0') {
                    definition->displayName = displayName;
                }

                definition->fontFiles = ParseCsv(fontFiles);
                definition->glyphRanges = ParseCsv(glyphRanges);
            }

            CSimpleIniA::TNamesDepend sections;
            ini.GetAllSections(sections);

            for (const auto& section : sections) {
                if (_stricmp(section.pItem, kLanguageMetaSection.data()) == 0) {
                    continue;
                }

                CSimpleIniA::TNamesDepend keys;
                ini.GetAllKeys(section.pItem, keys);

                for (const auto& key : keys) {
                    const auto value = ini.GetValue(section.pItem, key.pItem, "");
                    out[std::string(section.pItem) + "." + std::string(key.pItem)] = value;
                }
            }

            return true;
        }

        std::shared_ptr<const LanguageTable> ReadTable(const std::filesystem::path& directory, std::string_view code)
        {
            auto table = std::make_shared<LanguageTable>();
            if (!LoadLanguageFile(LanguagePath(directory, code), table->strings, &table->definition)) return {};
            // A metadata-only custom locale can deliberately use English strings.
            if (table->strings.empty() && table->definition.fontFiles.empty() && table->definition.glyphRanges.empty() &&
                table->definition.displayName == table->definition.code) return {};
            return table;
        }
    }

    bool Language::Load(std::string_view languageCode, const Diagnostic& diagnostic)
    {
        return LoadFromDirectory(ResolveLanguageDirectory(), languageCode, diagnostic);
    }

    bool Language::LoadFromDirectory(const std::filesystem::path& directory, std::string_view languageCode, const Diagnostic& diagnostic)
    {
        const ProfileScope profileScope(ProfileMetric::LanguageLoad);
        // Serialize reloads, never readers. Build a complete replacement before
        // publishing it. A failed load retains the last usable locale and fonts.
        std::lock_guard reload(reloadMutex);
        const auto report = [&](std::string message) { if (diagnostic) diagnostic(std::move(message)); };
        const auto code = languageCode.empty() ? std::string("en") : std::string(languageCode);
        if (!ValidLanguageCode(code)) {
            report("Invalid language code; retaining the current language");
            return false;
        }
        try {
            const auto previous = Read();
            const auto englishFile = ReadTable(directory, "en");
            auto english = englishFile;
            if (!english) {
                report("English language file unavailable; retaining the previous English fallback");
                english = previous->english;
            }
            auto selected = code == "en" ? englishFile : ReadTable(directory, code);
            if (!selected) {
                report("Requested language file unavailable; falling back to English");
                selected = englishFile;
            }
            if (!selected || (selected->strings.empty() && english->strings.empty())) {
                report("No usable language strings; retaining the current language");
                return false;
            }
            snapshots.Publish(std::make_shared<const LanguageSnapshot>(LanguageSnapshot{ std::move(selected), std::move(english) }));
            return true;
        } catch (const std::filesystem::filesystem_error& error) {
            report("Language load failed; retaining the current language: " + std::string(error.what()));
            return false;
        }
    }

    std::shared_ptr<const LanguageSnapshot> Language::Read() { return snapshots.Read(); }

    std::string Language::GetCopy(std::string_view section, std::string_view key)
    {
        const auto snapshot = Read();
        return std::string(snapshot->Get(section, key));
    }

    Language::Frame::Frame() : snapshot(Read()), previous(frameSnapshot) { frameSnapshot = snapshot.get(); }
    Language::Frame::~Frame() { frameSnapshot = previous; }

    std::string_view Language::FrameText(std::string_view section, std::string_view key)
    {
        if (!frameSnapshot) throw std::logic_error("Language::FrameText requires a Language::Frame owner");
        return frameSnapshot->Get(section, key);
    }

    std::string Language::GetCurrentLanguageCode()
    {
        if (frameSnapshot) return frameSnapshot->selected->definition.code;
        return Read()->selected->definition.code;
    }

    std::vector<Language::Definition> Language::ListAvailableLanguages()
    {
        return ListAvailableLanguages(ResolveLanguageDirectory());
    }

    std::vector<Language::Definition> Language::ListAvailableLanguages(const std::filesystem::path& directory)
    {
        std::vector<Definition> result;
        std::error_code error;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            if (!it->is_regular_file(error) || error) continue;
            if (_stricmp(it->path().extension().string().c_str(), ".ini") != 0) continue;
            const auto code = Stem(it->path());
            if (!ValidLanguageCode(code)) continue;
            if (const auto table = ReadTable(directory, code)) result.push_back(table->definition);
        }
        if (result.empty()) result.push_back({ .code = "en", .displayName = "English" });
        std::sort(result.begin(), result.end(), [](const Definition& left, const Definition& right) {
            const int order = _stricmp(left.code.c_str(), right.code.c_str());
            return order == 0 ? left.code < right.code : order < 0;
        });
        result.erase(std::unique(result.begin(), result.end(), [](const Definition& left, const Definition& right) {
            return _stricmp(left.code.c_str(), right.code.c_str()) == 0;
        }), result.end());
        return result;
    }
}
