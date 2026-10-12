#pragma once

#include "ShopPurchase.h"
#include "Features/Inventory/CashInventory.h"
#include "Protocol/CashInventoryPickupRequest.h"
#include "Protocol/CashInventoryPickupResponse.h"

namespace BubbleFighter::Server::Features::Shop
{
    struct CashPurchaseResult
    {
        ShopPurchaseResult Purchase;
        Inventory::CashInventory Pending;
        std::uint32_t CashCost = 0;
    };

    struct CashPickupResult
    {
        Characters::CharacterRecord Character;
        Inventory::CharacterInventory Inventory;
        Inventory::CashInventory Pending;
        Protocol::CashInventoryPickupResponse Response;
        std::uint32_t ChangedItemId = 0;
        std::vector<Protocol::TcpPacket> Packets;
    };

    class CashShop
    {
    public:
        // assignedSlotId is allocated by trusted persistence, never by C809.
        static CashPurchaseResult Purchase(const Protocol::ShopPurchaseRequest& request,
            const Characters::CharacterRecord& character, const Inventory::CharacterInventory& inventory,
            Inventory::CashInventory pending, std::uint32_t assignedSlotId, std::uint64_t observedAtUnixSeconds);
        static CashPickupResult Pickup(const Protocol::CashInventoryPickupRequest& request,
            const Characters::CharacterRecord& character, Inventory::CharacterInventory inventory,
            Inventory::CashInventory pending, std::uint64_t observedAtUnixSeconds);
    };
}
