#pragma once

#include "EquipmentView.h"
#include "Protocol/WeaponPresetChangeRequest.h"
#include "Protocol/WeaponPresetUpdateNotification.h"

namespace BubbleFighter::Server::Features::Inventory
{
    struct WeaponPresetState
    {
        // User-selected local policy: all five pages initially available.
        // This is not a recovered original account entitlement.
        static constexpr std::size_t InitialPages = 5;
        std::uint8_t ActiveIndex = 0;
        std::vector<std::uint32_t> PrimaryItems = std::vector<std::uint32_t>(InitialPages, 0);

        void Validate() const;
        // Ordinary equipped flags are authoritative for the active page. This
        // also imports existing equipment without granting a starter weapon.
        void CaptureActive(const CharacterInventory& inventory);
        Protocol::WeaponPresetUpdateNotification CreateNotification(const Characters::CharacterRecord& character,
            const CharacterInventory& inventory, std::uint64_t observedAtUnixSeconds) const;
        bool operator==(const WeaponPresetState&) const = default;
    };

    struct WeaponPresetChangeResult
    {
        CharacterInventory Inventory;
        WeaponPresetState Presets;
        std::vector<Protocol::TcpPacket> Packets;
    };

    WeaponPresetChangeResult PrepareWeaponPresetChange(const Characters::CharacterRecord& character,
        CharacterInventory inventory, WeaponPresetState presets, const Protocol::WeaponPresetChangeRequest& request,
        std::uint64_t observedAtUnixSeconds);
}
