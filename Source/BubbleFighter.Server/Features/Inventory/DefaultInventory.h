#pragma once

#include "EquipmentView.h"

namespace BubbleFighter::Server::Features::Inventory
{
    class DefaultInventory
    {
    public:
        // Local permanent starter policy: four verified native empty-slot
        // weapons plus the owned current character. Preserve existing records.
        static CharacterInventory Prepare(const Characters::CharacterRecord& character,
            CharacterInventory inventory, std::uint64_t observedAtUnixSeconds);
    };
}
