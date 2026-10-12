#include "DefaultInventory.h"
#include "InventoryGrant.h"
#include "ItemCatalog.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    CharacterInventory DefaultInventory::Prepare(const Characters::CharacterRecord& character,
        CharacterInventory inventory, std::uint64_t observedAtUnixSeconds)
    {
        (void)EquipmentView::Create(character, inventory, observedAtUnixSeconds);
        std::uint32_t occupiedSlots = 0, occupiedParts = 0;
        for (const auto& slot : inventory.Slots)
        {
            if (!slot.Equipped) { continue; }
            const auto& item = *ItemCatalog::Find(slot.ItemId); // validated above
            occupiedSlots |= item.SlotMask;
            occupiedParts |= item.PartMask | item.SecondaryPartMask;
        }
        // 0x010EAB10 selectors 0..3, initialized by 0x010EA690.
        // Permanent ownership is an explicit local policy, not an inferred
        // original-server reward or a grant of the entire resource catalog.
        const auto characterItem = 300000U + character.CharacterType;
        const std::array<std::uint32_t, 5> defaults{202006, 201005, 207001, 205033, characterItem};
        for (const auto id : defaults)
        {
            if (std::any_of(inventory.Slots.begin(), inventory.Slots.end(),
                [id](const auto& slot) { return slot.ItemId == id; })) { continue; }
            const auto* definition = ItemCatalog::Find(id);
            if (!definition || definition->Usage != InventoryUsage::Duration ||
                (definition->SlotMask == 0 && id != characterItem))
            { throw std::logic_error("Default inventory catalog changed."); }
            if (InventoryGrant::Apply(inventory, id, 0xFFFFFFFFU, 0xFFFFFFFFU, observedAtUnixSeconds) != InventoryGrantStatus::Granted)
            { throw std::logic_error("Default weapon grant failed."); }
            auto& added = inventory.Slots.back();
            const auto parts = definition->PartMask | definition->SecondaryPartMask;
            added.Equipped = (occupiedSlots & definition->SlotMask) == 0 && (occupiedParts & parts) == 0;
            if (added.Equipped)
            {
                occupiedSlots |= definition->SlotMask;
                occupiedParts |= parts;
            }
        }
        // Reject invalid eligibility/aggregate size before any database write.
        (void)EquipmentView::Create(character, inventory, observedAtUnixSeconds);
        (void)inventory.CreateNotification(character.AccountId, observedAtUnixSeconds).Encode();
        return inventory;
    }
}
