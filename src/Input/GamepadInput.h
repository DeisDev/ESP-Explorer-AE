#pragma once

#include "pch.h"

#include <cstdint>

namespace ESPExplorerAE
{
    class GamepadInput
    {
    public:
        static void Poll();
        static bool WasMenuTogglePressed();
        static bool WasTabNextPressed();
        static bool WasTabPrevPressed();
        static bool IsGamepadConnected();
        static bool IsUsingGamepad();
        static void ShowSteamKeyboard();
        static void CloseSteamKeyboard();
        static bool IsSteamKeyboardOpen();
        static void CheckSteamKeyboardResult(char* buffer, std::size_t bufferSize, std::string& value);

    private:
        static void UpdateImGuiNavInputs();

        static inline bool gamepadConnected{ false };
        static inline bool usingGamepad{ false };
        static inline bool menuTogglePressed{ false };
        static inline bool tabNextPressed{ false };
        static inline bool tabPrevPressed{ false };
        static inline bool steamKeyboardOpen{ false };

        static inline bool prevRB{ false };
        static inline bool prevLB{ false };
        static inline bool prevX{ false };
        static inline bool comboTriggered{ false };
    };
}
