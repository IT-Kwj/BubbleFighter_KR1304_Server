#pragma once

#include "InventorySlot.h"
#include "Protocol/CashInventoryUpdateNotification.h"

namespace BubbleFighter::Server::Features::Inventory
{
    struct CashInventorySlot
    {
        std::uint32_t SlotId = 0;
        std::uint32_t StockId = 0;
        std::uint32_t ItemId = 0;
        InventoryUsage Usage = InventoryUsage::Count;
        std::uint32_t Amount = 0;
        std::uint32_t MaxAmount = 0;
        std::uint32_t CashPrice = 0;
        std::uint64_t PurchasedAtUnixSeconds = 0;
        bool operator==(const CashInventorySlot&) const = default;
    };

    struct CashInventory
    {
        // Client UI adds 100000000 to cash IDs. Keep the local ID range
        // disjoint from ordinary items and avoid signed arithmetic overflow.
        static constexpr std::uint32_t MaxSlotId = 99'999'999;
        std::uint64_t AccountId = 0;
        std::uint64_t CharacterId = 0;
        std::vector<CashInventorySlot> Slots;
        Protocol::CashInventoryUpdateNotification CreateNotification(std::uint64_t authenticatedAccountId) const;
        bool operator==(const CashInventory&) const = default;
    };
}
