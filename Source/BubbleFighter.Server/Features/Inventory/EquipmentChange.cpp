#include "EquipmentChange.h"
#include "ItemCatalog.h"
#include "Protocol/InventoryViewUpdateNotification.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Inventory
{
    EquipmentChangeResult EquipmentChange::Prepare(const Characters::CharacterRecord& character,
        CharacterInventory inventory, std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAtUnixSeconds)
    {
        if (inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId ||
            itemIdentifier < 100000 || itemIdentifier >= 300000)
        { throw std::invalid_argument("Equipment request is outside the owned ordinary clothing/weapon path."); }
        (void)inventory.ToRecords(character.AccountId, observedAtUnixSeconds);
        const auto itemId = static_cast<std::uint32_t>(itemIdentifier);
        const auto target = std::find_if(inventory.Slots.begin(), inventory.Slots.end(),
            [&](const auto& slot) { return slot.ItemId == itemId; });
        const auto* definition = ItemCatalog::Find(itemId);
        if (target == inventory.Slots.end() || !definition || definition->Function != 0 || definition->Usage != target->Usage)
        { throw std::invalid_argument("Equipment request does not identify an owned supported item."); }
        if (equip)
        {
            // Explicit local replacement policy. The masks come from the
            // current catalog; the original server's conflict policy is unknown.
            for (auto& slot : inventory.Slots)
            {
                if (!slot.Equipped || slot.ItemId == itemId) { continue; }
                const auto* other = ItemCatalog::Find(slot.ItemId);
                if (!other || other->Usage != slot.Usage)
                { throw std::invalid_argument("Existing equipment has an unknown catalog definition."); }
                if ((other->SlotMask & definition->SlotMask) != 0 ||
                    ((other->PartMask | other->SecondaryPartMask) & (definition->PartMask | definition->SecondaryPartMask)) != 0)
                { slot.Equipped = false; }
            }
        }
        target->Equipped = equip;
        // Validation of the final state also allows an expired or disabled
        // ordinary item to be removed without granting it a new usable period.
        const auto view = EquipmentView::Create(character, inventory, observedAtUnixSeconds);
        std::vector<Protocol::TcpPacket> packets;
        packets.reserve(2);
        packets.push_back(inventory.CreateNotification(character.AccountId, observedAtUnixSeconds).Encode());
        packets.push_back(Protocol::InventoryViewUpdateNotification{view}.Encode());
        return {std::move(inventory), std::move(packets)};
    }
}
