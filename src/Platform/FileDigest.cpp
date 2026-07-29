#include "Platform/FileDigest.h"
#include "Core/ScopeExit.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <fstream>

namespace ESPExplorerAE
{
    std::optional<std::string> SHA256File(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        BCRYPT_ALG_HANDLE algorithm{};
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
        ScopeExit closeAlgorithm([&] { BCryptCloseAlgorithmProvider(algorithm, 0); });
        BCRYPT_HASH_HANDLE hash{};
        if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) return {};
        ScopeExit closeHash([&] { BCryptDestroyHash(hash); });
        std::array<unsigned char, 65536> bytes;
        while (file) {
            file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            if (file.gcount() && BCryptHashData(hash, bytes.data(), static_cast<ULONG>(file.gcount()), 0) < 0) return {};
        }
        if (!file.eof()) return {};
        std::array<unsigned char, 32> digest{};
        if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) return {};
        constexpr char hex[] = "0123456789abcdef";
        std::string result;
        result.reserve(digest.size() * 2);
        for (const auto value : digest) { result.push_back(hex[value >> 4]); result.push_back(hex[value & 15]); }
        return result;
    }
}
