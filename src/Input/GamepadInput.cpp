#include "pch.h"
#include "Input/GamepadInput.h"
#include "Platform/SteamKeyboard.h"


#include <imgui.h>

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

    }

    void GamepadInput::Poll(bool allowInput, bool keyboardMouseActivity)
    {
        EnsureXInputLoaded();

        static bool loggedConnectedState = false;
        static bool loggedUsingGamepadState = false;

        static bool waitForRelease{};
        static XINPUT_GAMEPAD previousInput{};
        menuTogglePressed = false;
        tabNextPressed = false;
        tabPrevPressed = false;

        if (!pXInputGetState) {
            UpdateImGuiNavInputs({});
            waitForRelease = true;
            if (gamepadConnected) {
                REX::DEBUG("{}", "Gamepad disconnected");
            }
            gamepadConnected = false;
            usingGamepad = false;
            previousInput = {};
            loggedConnectedState = false;
            loggedUsingGamepadState = false;
            return;
        }

        XINPUT_STATE state{};
        DWORD result = pXInputGetState(0, &state);

        if (result != ERROR_SUCCESS) {
            UpdateImGuiNavInputs({});
            waitForRelease = true;
            if (gamepadConnected) {
                REX::DEBUG("{}", "Gamepad disconnected");
            }
            gamepadConnected = false;
            usingGamepad = false;
            previousInput = {};
            ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
            prevRB = false;
            prevLB = false;
            prevX = false;
            comboTriggered = false;
            loggedConnectedState = false;
            loggedUsingGamepadState = false;
            return;
        }

        gamepadConnected = true;
        if (!loggedConnectedState) {
            REX::INFO("{}", "Gamepad connected");
            loggedConnectedState = true;
        }
        ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;

        const WORD buttons = state.Gamepad.wButtons;
        const auto& input = state.Gamepad;
        const auto changedAxis = [](SHORT current, SHORT previous, SHORT deadzone) {
            return current != previous && std::abs(static_cast<int>(current)) > deadzone;
        };
        const bool controllerActivity = (buttons & ~previousInput.wButtons) != 0 ||
            (input.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD && input.bLeftTrigger != previousInput.bLeftTrigger) ||
            (input.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD && input.bRightTrigger != previousInput.bRightTrigger) ||
            changedAxis(input.sThumbLX, previousInput.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ||
            changedAxis(input.sThumbLY, previousInput.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) ||
            changedAxis(input.sThumbRX, previousInput.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) ||
            changedAxis(input.sThumbRY, previousInput.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        previousInput = input;
        if (allowInput && keyboardMouseActivity) usingGamepad = false;
        if (!allowInput || waitForRelease) {
            UpdateImGuiNavInputs({});
            prevRB = false;
            prevLB = false;
            prevX = false;
            comboTriggered = false;
            waitForRelease = !allowInput || buttons != 0;
            return;
        }
        // Desktop events win when both devices are observed in the same frame.
        if (!keyboardMouseActivity && controllerActivity) usingGamepad = true;

        if (usingGamepad && !loggedUsingGamepadState) {
            REX::DEBUG("{}", "Input mode switched to gamepad");
        }
        loggedUsingGamepadState = usingGamepad;

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

        UpdateImGuiNavInputs(usingGamepad ? state : XINPUT_STATE{});
    }

    void GamepadInput::UpdateImGuiNavInputs(const XINPUT_STATE& state)
    {
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

    bool GamepadInput::ShowSteamKeyboard(std::uint32_t owner, const char* description, const char* existingText, std::size_t capacity)
    {
        return SteamKeyboard::Show(owner, description, existingText, capacity);
    }

    void GamepadInput::CloseSteamKeyboard()
    {
        SteamKeyboard::Abandon();
    }

    bool GamepadInput::IsSteamKeyboardOpen()
    {
        return SteamKeyboard::IsOpen();
    }

    bool GamepadInput::CheckSteamKeyboardResult(std::uint32_t owner, char* buffer, std::size_t bufferSize, std::string& value)
    {
        const auto result = SteamKeyboard::Take(owner);
        if (!result || !buffer || result->size() >= bufferSize) return false;
        // Reject oversized UTF-8 input in full instead of splitting a code point.
        std::memcpy(buffer, result->c_str(), result->size() + 1);
        value = *result;
        return true;
    }
}
