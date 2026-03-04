#pragma once

#include "Data/DataManager.h"
#include "Localization/Language.h"

#include <functional>

namespace ESPExplorerAE
{
    struct ContextMenuCallbacks
    {
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;
        using OpenItemGrantFn = std::function<void(const FormEntry&)>;
        using OpenGlobalValueFn = std::function<void(std::uint32_t)>;
        using ConfirmActionFn = std::function<void(std::string, std::string, std::function<void()>)>;

        LocalizeFn localize;
        OpenItemGrantFn openItemGrantPopup;
        OpenGlobalValueFn openGlobalValuePopup;
        ConfirmActionFn requestActionConfirmation;
        std::unordered_set<std::uint32_t>* favorites{ nullptr };
        int equipWeaponAmmoCount{ 200 };
    };

    class ContextMenu
    {
    public:
        static bool CanGiveItem(std::string_view category);
        static bool CanSpawn(std::string_view category);
        static bool CanTeleport(std::string_view category);
        static bool IsEquippable(std::string_view category);
        static bool IsSpellLike(std::string_view category);
        static bool IsPerk(std::string_view category);
        static bool IsQuest(std::string_view category);
        static bool IsWeather(std::string_view category);
        static bool IsSound(std::string_view category);
        static bool IsGlobal(std::string_view category);
        static bool IsOutfit(std::string_view category);
        static bool IsConstructible(std::string_view category);

        static const char* TryGetEditorID(std::uint32_t formID);

        static void Draw(const FormEntry& entry, const ContextMenuCallbacks& callbacks);
    };
}
