#include "Platform/HookTransaction.h"

#include <cstring>
#include <cstdint>
#include <cstddef>

namespace ESPExplorerAE
{
    bool PointerPatch::Install(void** address, void* next, const HookAPI& api)
    {
        if (!Restore(api) || retained || !address || !next || reinterpret_cast<std::uintptr_t>(address) % alignof(void*)) return false;
        slot = address;
        replacement = next;
        // Interlocked reads require writable memory, so first acquire protection.
        if (!api.protect(slot, sizeof(void*), PAGE_READWRITE, &originalProtection)) {
            slot = nullptr;
            return false;
        }
        protectionPending = true;
        previous = InterlockedCompareExchangePointer(slot, nullptr, nullptr);
        if (!previous || previous == replacement) {
            Restore(api);
            return false;
        }
        const bool exchanged = InterlockedCompareExchangePointer(slot, replacement, previous) == previous;
        DWORD ignored{};
        if (api.protect(slot, sizeof(void*), originalProtection, &ignored)) protectionPending = false;
        if (exchanged && !protectionPending) return true;
        Restore(api);
        return false;
    }

    bool PointerPatch::Restore(const HookAPI& api)
    {
        if (!slot) return true;
        DWORD ignored{};
        if (!protectionPending && !api.protect(slot, sizeof(void*), PAGE_READWRITE, &ignored)) return false;
        protectionPending = true;
        // Never overwrite a hook installed later by somebody else.
        const auto observed = InterlockedCompareExchangePointer(slot, previous, replacement);
        if (observed != replacement && observed != previous) retained = true;
        if (!api.protect(slot, sizeof(void*), originalProtection, &ignored)) return false;
        protectionPending = false;
        slot = nullptr;
        return true;
    }

    bool HookTransaction::Install(HWND nextWindow, WNDPROC nextProcedure, void** presentSlot, void* presentHook,
        void** cursorSlot, void* cursorHook)
    {
        if (ready) return true;
        if (!Restore() || windowRetained || present.RetainedByLaterHook() || cursor.RetainedByLaterHook() ||
            !nextWindow || !nextProcedure || !IsWindow(nextWindow)) return false;
        window = nextWindow;
        procedure = nextProcedure;
        SetLastError(ERROR_SUCCESS);
        previousWindow = reinterpret_cast<WNDPROC>(api.setWindow(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(procedure)));
        if (!previousWindow && GetLastError() != ERROR_SUCCESS) return false;
        windowPatched = true;
        if ((cursorSlot && !cursor.Install(cursorSlot, cursorHook, api)) || !present.Install(presentSlot, presentHook, api)) {
            Restore();
            return false;
        }
        ready = true;
        return true;
    }

    bool HookTransaction::Restore()
    {
        ready = false;
        const bool restoredPresent = present.Restore(api);
        const bool restoredCursor = cursor.Restore(api);
        bool restoredWindow = true;
        if (windowPatched) {
            const auto observed = IsWindow(window) ? reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC)) : nullptr;
            if (observed == procedure) {
                SetLastError(ERROR_SUCCESS);
                restoredWindow = api.setWindow(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previousWindow)) != 0 || GetLastError() == ERROR_SUCCESS;
            } else if (observed && observed != previousWindow) {
                // Reinstalling above a later hook that chains through us would
                // create a cycle. Remain a pass-through for this process.
                windowRetained = true;
            }
            if (restoredWindow) windowPatched = false;
        }
        return restoredPresent && restoredCursor && restoredWindow;
    }

    void** FindImport(HMODULE module, const char* library, const char* function)
    {
        if (!module) return nullptr;
        auto* base = reinterpret_cast<std::byte*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return nullptr;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return nullptr;
        const auto size = nt->OptionalHeader.SizeOfImage;
        const auto valid = [size](std::size_t offset, std::size_t count) { return offset < size && count <= size - offset; };
        const auto textAt = [&](DWORD offset) -> const char* {
            if (!valid(offset, 1)) return nullptr;
            const auto* text = reinterpret_cast<const char*>(base + offset);
            return std::memchr(text, 0, size - offset) ? text : nullptr;
        };
        const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!directory.VirtualAddress || !valid(directory.VirtualAddress, directory.Size)) return nullptr;
        for (std::size_t offset = directory.VirtualAddress; offset + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= directory.VirtualAddress + directory.Size;
             offset += sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
            const auto* entry = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + offset);
            if (!entry->Name) break;
            const auto* name = textAt(entry->Name);
            if (!name || _stricmp(name, library) || !entry->OriginalFirstThunk || !entry->FirstThunk) continue;
            for (std::size_t i = 0; valid(entry->OriginalFirstThunk + i, sizeof(IMAGE_THUNK_DATA64)) && valid(entry->FirstThunk + i, sizeof(void*)); i += sizeof(IMAGE_THUNK_DATA64)) {
                const auto* thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + entry->OriginalFirstThunk + i);
                if (!thunk->u1.AddressOfData) break;
                if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal) || thunk->u1.AddressOfData > size - sizeof(WORD)) continue;
                const auto* imported = textAt(static_cast<DWORD>(thunk->u1.AddressOfData + sizeof(WORD)));
                if (imported && std::strcmp(imported, function) == 0) return reinterpret_cast<void**>(base + entry->FirstThunk + i);
            }
        }
        return nullptr;
    }
}
