#include "Input/GamepadInput.h"

#include "Logging/Logger.h"

#include <imgui.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Xinput.h>

namespace ESPExplorerAE
{
    namespace
    {
        constexpr float STICK_DEADZONE = 0.20f;
        constexpr float TRIGGER_THRESHOLD = 0.12f;

        using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        XInputGetState_t pXInputGetState{ nullptr };
        bool xinputLoaded{ false };

        void EnsureXInputLoaded()
        {
            if (xinputLoaded) {
                return;
            }

            HMODULE xinput = LoadLibraryA("xinput1_4.dll");
            if (!xinput) {
                xinput = LoadLibraryA("xinput1_3.dll");
            }
            if (!xinput) {
                xinput = LoadLibraryA("xinput9_1_0.dll");
            }

            if (xinput) {
                pXInputGetState = reinterpret_cast<XInputGetState_t>(GetProcAddress(xinput, "XInputGetState"));
            }

            xinputLoaded = true;
        }

        float ApplyDeadzone(SHORT value, SHORT deadzone)
        {
            if (value > deadzone) {
                return static_cast<float>(value - deadzone) / static_cast<float>(32767 - deadzone);
            }
            if (value < -deadzone) {
                return static_cast<float>(value + deadzone) / static_cast<float>(32767 - deadzone);
            }
            return 0.0f;
        }

        struct SteamOverlayFuncs
        {
            using ShowGamepadTextInput_t = bool (*)(int inputMode, int lineInputMode, const char* description, unsigned int charMax, const char* existingText);
            using GetEnteredGamepadTextLength_t = unsigned int (*)();
            using GetEnteredGamepadTextInput_t = bool (*)(char* text, unsigned int maxLength);
            using IsOverlayEnabled_t = bool (*)();

            ShowGamepadTextInput_t ShowGamepadTextInput{ nullptr };
            GetEnteredGamepadTextLength_t GetEnteredGamepadTextLength{ nullptr };
            GetEnteredGamepadTextInput_t GetEnteredGamepadTextInput{ nullptr };
            IsOverlayEnabled_t IsOverlayEnabled{ nullptr };
            bool loaded{ false };
            bool available{ false };
        };

        SteamOverlayFuncs& GetSteamFuncs()
        {
            static SteamOverlayFuncs funcs;
            if (!funcs.loaded) {
                funcs.loaded = true;
                HMODULE steamModule = GetModuleHandleA("steam_api64.dll");
                if (!steamModule) {
                    steamModule = GetModuleHandleA("steam_api.dll");
                }

                if (steamModule) {
                    using SteamUtils_t = void* (*)();
                    auto pSteamUtils = reinterpret_cast<SteamUtils_t>(GetProcAddress(steamModule, "SteamAPI_SteamUtils_v010"));
                    if (!pSteamUtils) {
                        pSteamUtils = reinterpret_cast<SteamUtils_t>(GetProcAddress(steamModule, "SteamAPI_SteamUtils_v009"));
                    }

                    if (pSteamUtils) {
                        void* utils = pSteamUtils();
                        if (utils) {
                            using ShowGPI_t = bool(__thiscall*)(void*, int, int, const char*, unsigned int, const char*);
                            using GetGPTLength_t = unsigned int(__thiscall*)(void*);
                            using GetGPTInput_t = bool(__thiscall*)(void*, char*, unsigned int);
                            using IsOverlay_t = bool(__thiscall*)(void*);

                            static void* s_utils = utils;

                            funcs.ShowGamepadTextInput = [](int inputMode, int lineInputMode, const char* desc, unsigned int charMax, const char* existing) -> bool {
                                auto** vt = *reinterpret_cast<void***>(s_utils);
                                auto fn = reinterpret_cast<ShowGPI_t>(vt[34]);
                                return fn(s_utils, inputMode, lineInputMode, desc, charMax, existing);
                            };

                            funcs.GetEnteredGamepadTextLength = []() -> unsigned int {
                                auto** vt = *reinterpret_cast<void***>(s_utils);
                                auto fn = reinterpret_cast<GetGPTLength_t>(vt[35]);
                                return fn(s_utils);
                            };

                            funcs.GetEnteredGamepadTextInput = [](char* text, unsigned int maxLen) -> bool {
                                auto** vt = *reinterpret_cast<void***>(s_utils);
                                auto fn = reinterpret_cast<GetGPTInput_t>(vt[36]);
                                return fn(s_utils, text, maxLen);
                            };

                            funcs.available = true;
                            Logger::Info("Steam overlay keyboard functions loaded");
                        }
                    }

                    if (!funcs.available) {
                        using SteamUtilsFlat_ShowGPI = bool(*)(int, int, const char*, unsigned int, const char*);
                        using SteamUtilsFlat_GetGPTLen = unsigned int(*)();
                        using SteamUtilsFlat_GetGPTInput = bool(*)(char*, unsigned int);

                        auto showGPI = reinterpret_cast<SteamUtilsFlat_ShowGPI>(GetProcAddress(steamModule, "SteamAPI_ISteamUtils_ShowGamepadTextInput"));
                        auto getGPTLen = reinterpret_cast<SteamUtilsFlat_GetGPTLen>(GetProcAddress(steamModule, "SteamAPI_ISteamUtils_GetEnteredGamepadTextLength"));
                        auto getGPTInput = reinterpret_cast<SteamUtilsFlat_GetGPTInput>(GetProcAddress(steamModule, "SteamAPI_ISteamUtils_GetEnteredGamepadTextInput"));

                        if (showGPI && getGPTLen && getGPTInput) {
                            funcs.ShowGamepadTextInput = showGPI;
                            funcs.GetEnteredGamepadTextLength = getGPTLen;
                            funcs.GetEnteredGamepadTextInput = getGPTInput;
                            funcs.available = true;
                            Logger::Info("Steam overlay keyboard (flat API) loaded");
                        }
                    }
                }

                if (!funcs.available) {
                    Logger::Warn("Steam overlay keyboard not available");
                }
            }
            return funcs;
        }
    }

    void GamepadInput::Poll()
    {
        EnsureXInputLoaded();

        menuTogglePressed = false;
        tabNextPressed = false;
        tabPrevPressed = false;

        if (!pXInputGetState) {
            gamepadConnected = false;
            usingGamepad = false;
            return;
        }

        XINPUT_STATE state{};
        DWORD result = pXInputGetState(0, &state);

        if (result != ERROR_SUCCESS) {
            gamepadConnected = false;
            usingGamepad = false;
            ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
            prevRB = false;
            prevLB = false;
            prevX = false;
            comboTriggered = false;
            return;
        }

        gamepadConnected = true;
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;

        const WORD buttons = state.Gamepad.wButtons;
        const bool anyButton = buttons != 0 ||
            state.Gamepad.bLeftTrigger > 30 ||
            state.Gamepad.bRightTrigger > 30 ||
            std::abs(static_cast<int>(state.Gamepad.sThumbLX)) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            std::abs(static_cast<int>(state.Gamepad.sThumbLY)) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            std::abs(static_cast<int>(state.Gamepad.sThumbRX)) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
            std::abs(static_cast<int>(state.Gamepad.sThumbRY)) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

        if (anyButton) {
            usingGamepad = true;
        }

        const bool currentRB = (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        const bool currentLB = (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        const bool currentX = (buttons & XINPUT_GAMEPAD_X) != 0;

        tabNextPressed = currentRB && !prevRB;
        tabPrevPressed = currentLB && !prevLB;

        if (currentRB && currentX) {
            if (!comboTriggered) {
                menuTogglePressed = true;
                comboTriggered = true;
            }
        } else {
            comboTriggered = false;
        }

        prevRB = currentRB;
        prevLB = currentLB;
        prevX = currentX;

        UpdateImGuiNavInputs();
    }

    void GamepadInput::UpdateImGuiNavInputs()
    {
        if (!pXInputGetState) {
            return;
        }

        XINPUT_STATE state{};
        if (pXInputGetState(0, &state) != ERROR_SUCCESS) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        const WORD buttons = state.Gamepad.wButtons;

        auto MapButton = [&](ImGuiKey key, WORD xinputButton) {
            io.AddKeyEvent(key, (buttons & xinputButton) != 0);
        };

        MapButton(ImGuiKey_GamepadFaceDown, XINPUT_GAMEPAD_A);
        MapButton(ImGuiKey_GamepadFaceRight, XINPUT_GAMEPAD_B);
        MapButton(ImGuiKey_GamepadFaceLeft, XINPUT_GAMEPAD_X);
        MapButton(ImGuiKey_GamepadFaceUp, XINPUT_GAMEPAD_Y);
        MapButton(ImGuiKey_GamepadDpadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
        MapButton(ImGuiKey_GamepadDpadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
        MapButton(ImGuiKey_GamepadDpadUp, XINPUT_GAMEPAD_DPAD_UP);
        MapButton(ImGuiKey_GamepadDpadDown, XINPUT_GAMEPAD_DPAD_DOWN);
        MapButton(ImGuiKey_GamepadL1, XINPUT_GAMEPAD_LEFT_SHOULDER);
        MapButton(ImGuiKey_GamepadR1, XINPUT_GAMEPAD_RIGHT_SHOULDER);
        MapButton(ImGuiKey_GamepadL3, XINPUT_GAMEPAD_LEFT_THUMB);
        MapButton(ImGuiKey_GamepadR3, XINPUT_GAMEPAD_RIGHT_THUMB);
        MapButton(ImGuiKey_GamepadStart, XINPUT_GAMEPAD_START);
        MapButton(ImGuiKey_GamepadBack, XINPUT_GAMEPAD_BACK);

        auto MapAnalog = [&](ImGuiKey key, float value, float deadzone) {
            io.AddKeyAnalogEvent(key, value > deadzone, value > deadzone ? (value - deadzone) / (1.0f - deadzone) : 0.0f);
        };

        float leftX = ApplyDeadzone(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        float leftY = ApplyDeadzone(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        float rightX = ApplyDeadzone(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        float rightY = ApplyDeadzone(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

        MapAnalog(ImGuiKey_GamepadLStickLeft, leftX < 0 ? -leftX : 0.0f, STICK_DEADZONE);
        MapAnalog(ImGuiKey_GamepadLStickRight, leftX > 0 ? leftX : 0.0f, STICK_DEADZONE);
        MapAnalog(ImGuiKey_GamepadLStickUp, leftY > 0 ? leftY : 0.0f, STICK_DEADZONE);
        MapAnalog(ImGuiKey_GamepadLStickDown, leftY < 0 ? -leftY : 0.0f, STICK_DEADZONE);

        MapAnalog(ImGuiKey_GamepadRStickLeft, rightX < 0 ? -rightX : 0.0f, STICK_DEADZONE);
        MapAnalog(ImGuiKey_GamepadRStickRight, rightX > 0 ? rightX : 0.0f, STICK_DEADZONE);
        MapAnalog(ImGuiKey_GamepadRStickUp, rightY > 0 ? rightY : 0.0f, STICK_DEADZONE);
        MapAnalog(ImGuiKey_GamepadRStickDown, rightY < 0 ? -rightY : 0.0f, STICK_DEADZONE);

        float leftTrigger = static_cast<float>(state.Gamepad.bLeftTrigger) / 255.0f;
        float rightTrigger = static_cast<float>(state.Gamepad.bRightTrigger) / 255.0f;
        MapAnalog(ImGuiKey_GamepadL2, leftTrigger, TRIGGER_THRESHOLD);
        MapAnalog(ImGuiKey_GamepadR2, rightTrigger, TRIGGER_THRESHOLD);
    }

    bool GamepadInput::WasMenuTogglePressed()
    {
        return menuTogglePressed;
    }

    bool GamepadInput::WasTabNextPressed()
    {
        return tabNextPressed;
    }

    bool GamepadInput::WasTabPrevPressed()
    {
        return tabPrevPressed;
    }

    bool GamepadInput::IsGamepadConnected()
    {
        return gamepadConnected;
    }

    bool GamepadInput::IsUsingGamepad()
    {
        return usingGamepad;
    }

    void GamepadInput::ShowSteamKeyboard()
    {
        auto& funcs = GetSteamFuncs();
        if (!funcs.available || !funcs.ShowGamepadTextInput) {
            Logger::Verbose("Steam keyboard not available");
            return;
        }

        if (steamKeyboardOpen) {
            return;
        }

        if (funcs.ShowGamepadTextInput(0, 0, "Search", 256, "")) {
            steamKeyboardOpen = true;
            Logger::Verbose("Steam keyboard opened");
        }
    }

    void GamepadInput::CloseSteamKeyboard()
    {
        steamKeyboardOpen = false;
    }

    bool GamepadInput::IsSteamKeyboardOpen()
    {
        return steamKeyboardOpen;
    }

    void GamepadInput::CheckSteamKeyboardResult(char* buffer, std::size_t bufferSize, std::string& value)
    {
        if (!steamKeyboardOpen) {
            return;
        }

        auto& funcs = GetSteamFuncs();
        if (!funcs.available) {
            steamKeyboardOpen = false;
            return;
        }

        unsigned int length = funcs.GetEnteredGamepadTextLength();
        if (length > 0 && length < static_cast<unsigned int>(bufferSize)) {
            char tempBuf[512]{};
            if (funcs.GetEnteredGamepadTextInput(tempBuf, sizeof(tempBuf))) {
                strncpy_s(buffer, bufferSize, tempBuf, _TRUNCATE);
                value = buffer;
            }
        }

        steamKeyboardOpen = false;
    }
}
