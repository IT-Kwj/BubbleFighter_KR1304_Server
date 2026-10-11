#pragma once

#include "EquipmentView.h"

namespace BubbleFighter::Server::Features::Inventory
{
    struct EquipmentChangeResult
    {
        CharacterInventory Inventory;
        std::vector<Protocol::TcpPacket> Packets;
    };

    class EquipmentChange
    {
    public:
        // Pure preparation: validates owned ordinary clothing/weapons, removes
        // conflicting positions and encodes the entire update before any write.
        // Character selection, ArticleSlot and special functions remain separate.
        static EquipmentChangeResult Prepare(const Characters::CharacterRecord& character,
            CharacterInventory inventory, std::uint64_t itemIdentifier, bool equip,
            std::uint64_t observedAtUnixSeconds);
    };
}
