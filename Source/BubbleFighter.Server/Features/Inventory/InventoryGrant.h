#pragma once

#include "InventorySlot.h"

namespace BubbleFighter::Server::Features::Inventory
{
    enum class InventoryGrantStatus { Granted, AlreadyOwned, LimitExceeded };

    // Applies an already authorized grant. This does not validate a shop offer,
    // accept client quantities, debit currency or persist anything.
    class InventoryGrant
    {
    public:
        static InventoryGrantStatus Apply(CharacterInventory& inventory, std::uint32_t itemId,
            std::uint32_t amount, std::uint32_t maxAmount, std::uint64_t observedAtUnixSeconds);
    };
}
