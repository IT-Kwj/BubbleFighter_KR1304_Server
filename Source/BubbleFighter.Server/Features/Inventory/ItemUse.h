#pragma once
#include "ItemCatalog.h"
#include "Features/Characters/CharacterStore.h"
#include "Protocol/ItemUseRequest.h"
#include "Protocol/ItemUseResponse.h"

namespace BubbleFighter::Server::Features::Inventory
{
    struct ItemUseResult
    {
        Characters::CharacterRecord Character;
        CharacterInventory Inventory;
        Protocol::ItemUseResponse Response;
        std::vector<Protocol::TcpPacket> Packets;
    };

    class ItemUse
    {
    public:
        static bool SupportsRename(const ItemDefinition& item) noexcept;
        // Pure preparation; persistence must check the unique name and commit
        // the name and card decrement together before publishing any packets.
        static ItemUseResult PrepareRename(const Characters::CharacterRecord& character,
            CharacterInventory inventory, const Protocol::ItemUseRequest& request,
            std::uint64_t observedAtUnixSeconds, bool nameTaken = false);
    };
}
