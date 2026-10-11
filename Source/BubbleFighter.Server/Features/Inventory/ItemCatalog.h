#pragma once

#include "InventorySlot.h"

#include <span>

namespace BubbleFighter::Server::Features::Inventory
{
    // E059: current Config/ShopItem2.xml, interpreted against 0x00A17CF0.
    // Sales/price metadata is not an entitlement or a grant to this account.
    struct ItemDefinition
    {
        std::uint32_t ItemId;
        InventoryUsage Usage;
        std::uint32_t AllowedCharacters;
        std::uint32_t RequiredLevel;
        std::uint32_t SlotMask;
        std::uint32_t PartMask;
        std::uint32_t SecondaryPartMask;
        std::uint32_t SocketMask;
        std::uint32_t Function;
        std::uint32_t Parameter;
        bool Enabled;
    };

    class ItemCatalog
    {
    public:
        static std::span<const ItemDefinition> Entries() noexcept;
        static const ItemDefinition* Find(std::uint32_t itemId) noexcept;
    };
}
