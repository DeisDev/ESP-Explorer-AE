#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    class FormActions
    {
    public:
        static constexpr std::uint32_t kStimpakFormID = 0x00023736;
        static constexpr std::uint32_t kLockpickFormID = 0x0000000A;

        static void CopyFormID(std::uint32_t formID);
        static void GiveToPlayer(std::uint32_t formID, std::uint32_t count);
        static void GiveToPlayerWithAmmo(std::uint32_t formID, std::uint32_t itemCount, std::uint32_t ammoFormID, std::uint32_t ammoCount);
        static std::uint32_t GetWeaponAmmoFormID(std::uint32_t weaponFormID);
        static bool AddAmmoForCurrentWeapon(std::uint32_t ammoCount);
        static void SpawnAtPlayer(std::uint32_t formID, std::uint32_t count);
        static void PlaceAtPlayer(std::uint32_t formID, std::uint32_t count);
        static void AddSpellToPlayer(std::uint32_t formID);
        static void RemoveSpellFromPlayer(std::uint32_t formID);
        static void AddPerkToPlayer(std::uint32_t formID);
        static void RemovePerkFromPlayer(std::uint32_t formID);
        static int AddOutfitItemsToPlayer(std::uint32_t formID);
        static bool AddConstructedItemToPlayer(std::uint32_t formID);
        static void ExecuteConsoleCommand(std::string_view command);
        static void TeleportToCell(std::uint32_t formID);
        static bool CanUndoLastAction();
        static void UndoLastAction();
    };
}
