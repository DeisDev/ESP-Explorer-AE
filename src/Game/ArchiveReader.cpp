#include "Game/ArchiveReader.h"
#include "Game/ArchiveRegistryView.h"
#include "pch.h"

#include <atomic>
#include <cstring>
#include <initializer_list>

namespace ESPExplorerAE
{
    namespace
    {
        struct Binding
        {
            const ArchiveRegistry::View* registry{};
            std::uint32_t* lock{};
            std::uintptr_t generalSingleton{};
            std::uintptr_t textureSingleton{};
        };

        bool Bytes(std::uintptr_t address, std::initializer_list<std::uint8_t> expected)
        {
            return std::memcmp(reinterpret_cast<const void*>(address), expected.begin(), expected.size()) == 0;
        }

        bool RelativeTarget(std::uintptr_t instruction, std::size_t displacement, std::size_t length, std::uintptr_t target)
        {
            std::int32_t offset{};
            std::memcpy(&offset, reinterpret_cast<const void*>(instruction + displacement), sizeof(offset));
            return instruction + length + offset == target;
        }

        template <class T>
        T Read(std::uintptr_t address)
        {
            T value{};
            std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
            return value;
        }

        Binding Resolve()
        {
            // Verified against 1.11.240 registration listeners 2192349/2192350,
            // insert 2192371, and __ArchiveClearRegistrationListener 2192264.
            // Resolve each accepted runtime through its own Address Library;
            // refuse an incompatible body/layout instead of using the old
            // 256-entry Archive2::Index or the pinned BSTHashMap allocator.
            const REL::Relocation<std::uintptr_t> registry{ REL::ID{2661344} };
            const REL::Relocation<std::uintptr_t> lock{ REL::ID{4796409} };
            const REL::Relocation<std::uintptr_t> registered{ REL::ID{2192349} };
            const REL::Relocation<std::uintptr_t> textureRegistered{ REL::ID{2192350} };
            const REL::Relocation<std::uintptr_t> clear{ REL::ID{2192264} };
            const REL::Relocation<std::uintptr_t> insert{ REL::ID{2192371} };
            for (const auto listener : {registered.address(), textureRegistered.address()}) {
                if (!Bytes(listener + 0x1A, {0xF0, 0x0F, 0xB1, 0x2D}) ||
                    !RelativeTarget(listener + 0x1A, 4, 8, lock.address()) ||
                    !Bytes(listener + 0x64, {0x48, 0x8D, 0x0D}) ||
                    !RelativeTarget(listener + 0x64, 3, 7, registry.address())) return {};
            }
            if (!Bytes(clear.address() + 0x1A, {0xF0, 0x0F, 0xB1, 0x35}) ||
                !RelativeTarget(clear.address() + 0x1A, 4, 8, lock.address()) ||
                !Bytes(clear.address() + 0x53, {0x48, 0x8D, 0x0D}) ||
                !RelativeTarget(clear.address() + 0x53, 3, 7, registry.address()) ||
                !Bytes(insert.address() + 0x40, {0x4C, 0x8B, 0x73, 0x20}) ||
                !Bytes(insert.address() + 0x4D, {0x44, 0x8B, 0x4B, 0x04}) ||
                !Bytes(insert.address() + 0x5A, {0x48, 0x8D, 0x04, 0x49}) ||
                !Bytes(insert.address() + 0x5E, {0x49, 0x83, 0x7C, 0xC6, 0x10, 0x00})) return {};
            // Startup INI archives precede the TESDataHandler name listener.
            // Include the GNRL index and DX10 streamer's retained data files.
            const REL::Relocation<std::uintptr_t> generalSingleton{ REL::ID{2703380} };
            const REL::Relocation<std::uintptr_t> textureSingleton{ REL::ID{2704215} };
            const REL::Relocation<std::uintptr_t> generalCtor{ REL::ID{2269342} };
            const REL::Relocation<std::uintptr_t> textureCtor{ REL::ID{2275505} };
            const REL::Relocation<std::uintptr_t> generalAdd{ REL::ID{2269366} };
            const REL::Relocation<std::uintptr_t> generalEvent{ REL::ID{2269414} };
            const REL::Relocation<std::uintptr_t> textureEvent{ REL::ID{2275558} };
            const REL::Relocation<std::uintptr_t> textureClear{ REL::ID{2275556} };
            const REL::Relocation<std::uintptr_t> looseStreamVtable{ RE::VTABLE::BSResource____LooseFileStream[1] };
            const REL::Relocation<std::uintptr_t> looseStreamName{ REL::ID{2269634} };
            if (!Bytes(generalCtor.address() + 0x14, {0x48, 0x89, 0x0D}) ||
                !RelativeTarget(generalCtor.address() + 0x14, 3, 7, generalSingleton.address()) ||
                !Bytes(generalCtor.address() + 0x3A, {0x4C, 0x8D, 0x75, 0x08}) ||
                !Bytes(generalCtor.address() + 0x130, {0xB9, 0x00, 0x04, 0x00, 0x00}) ||
                !Bytes(generalAdd.address() + 0x18, {0x48, 0x83, 0xC1, 0x30}) ||
                !Bytes(generalAdd.address() + 0x42, {0x3B, 0xB7, 0x30, 0x70, 0x00, 0x00}) ||
                !Bytes(generalEvent.address() + 0x2F, {0x48, 0x8D, 0x9D, 0x58, 0x70, 0x00, 0x00}) ||
                !Bytes(textureCtor.address() + 0x40, {0x48, 0x89, 0x15}) ||
                !RelativeTarget(textureCtor.address() + 0x40, 3, 7, textureSingleton.address()) ||
                !Bytes(textureCtor.address() + 0x236, {0xB9, 0x00, 0x04, 0x00, 0x00}) ||
                !Bytes(textureCtor.address() + 0x414, {0x48, 0x8D, 0x4F, 0x58}) ||
                !Bytes(textureEvent.address() + 0x114, {0x48, 0x81, 0xC3, 0x58, 0xCD, 0x09, 0x00}) ||
                !Bytes(textureClear.address() + 0x8A, {0x48, 0x8B, 0xBE, 0x80, 0xCD, 0x09, 0x00}) ||
                !Bytes(textureClear.address() + 0x96, {0x8B, 0x86, 0x90, 0xCD, 0x09, 0x00}) ||
                Read<std::uintptr_t>(looseStreamVtable.address() + 0x78) != looseStreamName.address()) return {};
            return {reinterpret_cast<const ArchiveRegistry::View*>(registry.address()), reinterpret_cast<std::uint32_t*>(lock.address()),
                generalSingleton.address(), textureSingleton.address()};
        }

        struct ReadLock
        {
            explicit ReadLock(std::uintptr_t address) : lock(reinterpret_cast<RE::BSReadWriteLock*>(address))
            {
                if (!lock->try_lock_read()) lock = nullptr;
            }
            ~ReadLock() { if (lock) lock->unlock_read(); }
            RE::BSReadWriteLock* lock;
        };
    }

    std::optional<std::vector<std::string>> ArchiveReader::Capture()
    {
        static const auto binding = Resolve();
        if (!binding.registry || !binding.lock) return {};
        // The registration, removal, rename and clear paths all use this
        // non-reentrant 32-bit lock (0 -> 1). Do not stall the game-task pump.
        std::atomic_ref lock(*binding.lock);
        std::uint32_t expected{};
        if (!lock.compare_exchange_strong(expected, 1, std::memory_order_acquire)) return {};
        struct Unlock
        {
            std::atomic_ref<std::uint32_t> lock;
            ~Unlock() { lock.store(0, std::memory_order_release); }
        } unlock{lock};
        const auto general = Read<std::uintptr_t>(binding.generalSingleton);
        const auto textures = Read<std::uintptr_t>(binding.textureSingleton);
        if (!general || !textures) return {};
        ReadLock generalLock(general + 0x7060);
        if (!generalLock.lock) return {};
        ReadLock textureLock(textures + 0x9CDB0);
        if (!textureLock.lock) return {};
        const auto generalCount = Read<std::uint32_t>(general + 0x7038);
        const auto textureCount = Read<std::uint32_t>(textures + 0x9CDF0);
        const auto textureStreams = Read<const void* const*>(textures + 0x9CDE0);
        if (generalCount > 1024 || textureCount > 1024 || (textureCount && !textureStreams)) return {};
        std::unordered_set<const void*> seen;
        auto names = ArchiveRegistry::Capture(*binding.registry, [&](const ArchiveRegistry::Entry& entry) {
            // The key remains owned by the locked registry. Copy only its
            // characters; never acquire, close, or release a stream handle.
            seen.insert(entry.stream);
            return reinterpret_cast<const RE::BSFixedString*>(&entry.name)->c_str();
        });
        if (!names) return {};
        const auto streamName = [](const void* stream) -> std::optional<std::string> {
            // Load 2269272 and remove 2192335 use bit 3 at StreamBase +0x10
            // for the opened state. The pinned flags member is at the old +C.
            auto& flags = *reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(stream) + 0x10);
            if (!(std::atomic_ref(flags).load(std::memory_order_relaxed) & 8)) return std::string{};
            RE::BSFixedString name;
            // GetName only dispatches DoGetName (slot 0F), whose ABI matches
            // the inspected loose-file stream vtable. No I/O or stream retain.
            if (!static_cast<const RE::BSResource::Stream*>(stream)->GetName(name) || name.empty() || name.length() > 1024) return {};
            return std::string(name.c_str());
        };
        if (!ArchiveRegistry::AppendStreams(*names, {reinterpret_cast<const void* const*>(general + 0x38), generalCount}, seen, streamName) ||
            !ArchiveRegistry::AppendStreams(*names, {textureStreams, textureCount}, seen, streamName)) return {};
        std::ranges::sort(*names);
        return names;
    }
}
