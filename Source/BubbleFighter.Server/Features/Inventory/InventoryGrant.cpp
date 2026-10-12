#include "InventoryGrant.h"
#include "ItemCatalog.h"
#include "ItemUse.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    InventoryGrantStatus InventoryGrant::Apply(CharacterInventory& inventory, std::uint32_t itemId,
        std::uint32_t amount, std::uint32_t maxAmount, std::uint64_t observedAtUnixSeconds)
    {
        (void)inventory.ToRecords(inventory.AccountId, observedAtUnixSeconds);
        const auto* definition = ItemCatalog::Find(itemId);
        // Owning a catalog item does not execute its special function. ItemUse
        // and EquipmentChange retain their own effect/eligibility validation.
        if (!definition || amount == 0 ||
            (definition->Usage != InventoryUsage::Count && definition->Usage != InventoryUsage::Duration && definition->Usage != InventoryUsage::Life))
        { throw std::invalid_argument("Unsupported trusted ordinary item grant."); }
        auto found = std::find_if(inventory.Slots.begin(), inventory.Slots.end(),
            [&](const auto& slot) { return slot.ItemId == itemId; });
        InventorySlot changed;
        changed.ItemId = itemId;
        changed.Usage = definition->Usage;
        if (found != inventory.Slots.end())
        {
            if (found->Usage != definition->Usage) { throw std::invalid_argument("Stored grant usage differs from catalog."); }
            if (found->Usage == InventoryUsage::Duration && !found->ExpiresAtUnixSeconds)
            { return InventoryGrantStatus::AlreadyOwned; }
            changed = *found;
        }
        if (changed.Revision == (std::numeric_limits<std::uint64_t>::max)())
        { throw std::runtime_error("Grant inventory revision exhausted."); }
        if (changed.Usage == InventoryUsage::Count || changed.Usage == InventoryUsage::Life)
        {
            // Life is carried in the ordinary quantity field (E053). The
            // resource maxDuration is not established as a durability cap.
            const auto limit = changed.Usage == InventoryUsage::Life ? InventorySlot::MaxQuantity :
                (std::min)(maxAmount, InventorySlot::MaxQuantity);
            if (changed.Quantity > limit || amount > limit - changed.Quantity)
            { return InventoryGrantStatus::LimitExceeded; }
            changed.Quantity += amount;
        }
        else if (amount == 0xFFFFFFFFU)
        {
            changed.Quantity = 1;
            changed.ExpiresAtUnixSeconds.reset();
        }
        else
        {
            const auto days = (std::min<std::uint64_t>)(maxAmount, InventorySlot::MaxFiniteSeconds / 86'400);
            const auto base = (std::max)(observedAtUnixSeconds, changed.ExpiresAtUnixSeconds.value_or(observedAtUnixSeconds));
            const auto seconds = static_cast<std::uint64_t>(amount) * 86'400;
            if (amount > days || base - observedAtUnixSeconds > days * 86'400 - seconds ||
                seconds > InventorySlot::MaxUnixSeconds - base)
            { return InventoryGrantStatus::LimitExceeded; }
            changed.Quantity = 1;
            changed.ExpiresAtUnixSeconds = base + seconds;
        }
        if (found == inventory.Slots.end()) { inventory.Slots.push_back(std::move(changed)); }
        else { *found = std::move(changed); }
        return InventoryGrantStatus::Granted;
    }
}
