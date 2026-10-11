#include "InventoryRefresh.h"
#include "Features/Scenes/SceneRoster.h"

#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    std::vector<Protocol::TcpPacket> InventoryRefresh::Prepare(const Characters::CharacterRecord& character,
        std::uint64_t authenticatedAccountId, const CharacterInventory& inventory,
        const Protocol::PartySupplementaryRecord& equipment,
        const Protocol::WeaponPresetUpdateNotification& presets, std::uint64_t observedAtUnixSeconds)
    {
        if (inventory.AccountId != authenticatedAccountId || inventory.CharacterId != character.CharacterId ||
            equipment.Key != character.CharacterId || presets.PlayerId != character.CharacterId ||
            presets.Records.size() > MaxPresetRecords)
        { throw std::invalid_argument("Inventory refresh has inconsistent ownership or presets."); }

        const auto view = Scenes::SceneRoster::CreateInventoryView(authenticatedAccountId, character, equipment);
        const auto bag = inventory.CreateNotification(authenticatedAccountId, observedAtUnixSeconds);
        // Local refresh ordering: apply inventory before the final view update.
        // S802 updates the bag/UI; S837 uses the established player's view;
        // S806 refreshes the inventory preview through 0x009AF2F0/0x005F50F0.
        // No S3/S117: reading the bag must not clear identity or switch stages.
        std::vector<Protocol::TcpPacket> packets;
        packets.reserve(3);
        packets.push_back(bag.Encode());
        packets.push_back(presets.Encode());
        packets.push_back(view.Encode());
        return packets;
    }
}
