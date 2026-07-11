#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace ESPExplorerAE
{
    // The owner serializes installation and reading chain targets. No destructor
    // unhooks: DLL detach is not a safe teardown context, and later hooks may
    // still chain through our entry points after a conditional restoration.
    struct HookAPI
    {
        decltype(&VirtualProtect) protect{ &VirtualProtect };
        decltype(&SetWindowLongPtrW) setWindow{ &SetWindowLongPtrW };
    };

    class PointerPatch
    {
    public:
        bool Install(void** address, void* replacement, const HookAPI& api);
        bool Restore(const HookAPI& api);
        void* Previous() const { return previous; }
        bool RetainedByLaterHook() const { return retained; }

    private:
        void** slot{};
        void* previous{};
        void* replacement{};
        DWORD originalProtection{};
        bool protectionPending{};
        bool retained{};
    };

    class HookTransaction
    {
    public:
        explicit HookTransaction(HookAPI api = {}) : api(api) {}
        bool Install(HWND window, WNDPROC procedure, void** presentSlot, void* presentHook,
            void** cursorSlot, void* cursorHook);
        bool Restore();
        bool Ready() const { return ready; }
        WNDPROC PreviousWindow() const { return previousWindow; }
        void* PreviousPresent() const { return present.Previous(); }
        void* PreviousCursor() const { return cursor.Previous(); }

    private:
        HookAPI api;
        PointerPatch present;
        PointerPatch cursor;
        HWND window{};
        WNDPROC procedure{};
        WNDPROC previousWindow{};
        bool windowPatched{};
        bool windowRetained{};
        bool ready{};
    };

    // Locate the import by name so an existing IAT hook becomes our chain target.
    // A missing/stripped name table makes this optional hook unavailable.
    void** FindImport(HMODULE module, const char* library, const char* function);
}
