#pragma once

#include "InventorySlot.h"
#include "EquipmentChange.h"

namespace BubbleFighter::Server::Features::Inventory
{
    class InventoryStore
    {
    public:
        virtual ~InventoryStore() = default;
        // null = missing/disabled/foreign character, present empty = loaded empty.
        // Database failures throw; they must never be treated as an empty bag.
        virtual std::optional<CharacterInventory> Load(std::uint64_t authenticatedAccountId,
            std::uint64_t characterId, std::uint64_t observedAtUnixSeconds) = 0;
        // Trusted server callers only, not a client grant API. Revision 0 inserts,
        // otherwise compare-and-swap; false means unavailable ownership or stale
        // revision. Publish the new revision only after committing the write.
        virtual bool Save(std::uint64_t authenticatedAccountId, std::uint64_t characterId,
            InventorySlot& slot, std::uint64_t observedAtUnixSeconds) = 0;
        // Locks the owner, reloads all rows, prepares the complete wire update,
        // then commits every changed flag together. No grants or partial save.
        virtual EquipmentChangeResult ChangeEquipment(const Characters::CharacterRecord& character,
            std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAtUnixSeconds) = 0;
    };
}
