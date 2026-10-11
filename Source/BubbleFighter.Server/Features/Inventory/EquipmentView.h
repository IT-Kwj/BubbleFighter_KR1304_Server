#pragma once

#include "InventorySlot.h"
#include "Features/Characters/CharacterStore.h"
#include "Protocol/UpdatePartyNotification.h"

namespace BubbleFighter::Server::Features::Inventory
{
    class EquipmentView
    {
    public:
        // Builds ordinary clothing/weapon/current-character state from trusted
        // persistence and the current client catalog. Never grants starter gear.
        static Protocol::PartySupplementaryRecord Create(const Characters::CharacterRecord& character,
            const CharacterInventory& inventory, std::uint64_t observedAtUnixSeconds);
    };
}
