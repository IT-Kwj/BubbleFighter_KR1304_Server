#pragma once

#include "InventorySlot.h"
#include "Features/Characters/CharacterStore.h"
#include "Protocol/UpdatePartyNotification.h"
#include "Protocol/WeaponPresetUpdateNotification.h"

namespace BubbleFighter::Server::Features::Inventory
{
    class InventoryRefresh
    {
    public:
        // 0x0081B820 narrows the count to uint8. This is a safety ceiling,
        // not a recovered entitlement to 255 preset pages.
        static constexpr std::size_t MaxPresetRecords = 255;

        // Requires an established menu/plaza context. A successful trusted
        // load may be empty; a failed load must never become an empty bag.
        // Encodes the entire batch before the caller can publish any packet.
        static std::vector<Protocol::TcpPacket> Prepare(const Characters::CharacterRecord& character,
            std::uint64_t authenticatedAccountId, const CharacterInventory& inventory,
            const Protocol::PartySupplementaryRecord& equipment,
            const Protocol::WeaponPresetUpdateNotification& presets, std::uint64_t observedAtUnixSeconds);
    };
}
