#pragma once

#include "Features/Characters/CharacterStore.h"
#include "Features/Inventory/InventorySlot.h"
#include "Protocol/ShopPurchaseRequest.h"
#include "Protocol/ShopPurchaseResponse.h"

namespace BubbleFighter::Server::Features::Shop
{
    struct ShopPurchaseResult
    {
        Characters::CharacterRecord Character;
        Inventory::CharacterInventory Inventory;
        Protocol::ShopPurchaseResponse Response;
        std::vector<Protocol::TcpPacket> Packets;
        std::uint32_t LucciCost = 0;
        std::uint32_t ChangedItemId = 0;
    };

    class ShopPurchase
    {
    public:
        // Pure transaction preparation. Current support: published ordinary
        // Lucci items with known count/day usage. Cash pickup, bundles and
        // special functions require their own grant flows before acceptance.
        // Storage must supply locked, current balances/inventory and commit
        // both changes atomically only after all packets have been encoded.
        static ShopPurchaseResult Prepare(const Protocol::ShopPurchaseRequest& request,
            const Characters::CharacterRecord& character, Inventory::CharacterInventory inventory,
            std::uint64_t observedAtUnixSeconds);
    };
}
