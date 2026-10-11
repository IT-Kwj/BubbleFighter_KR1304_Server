#include "MenuInitialization.h"
#include "Features/Characters/CharacterProfile.h"
#include "Features/Scenes/SceneRoster.h"

#include <array>
#include <stdexcept>
#include <utility>

namespace BubbleFighter::Server::Features::Authentication
{
    std::vector<Protocol::TcpPacket> MenuInitialization::Prepare(const Characters::CharacterRecord& character,
        std::uint64_t authenticatedAccountId, MenuInitializationData data, std::uint64_t observedAtUnixSeconds)
    {
        if (data.Scene.Descriptor.SessionType != 1 || data.Inventory.AccountId != authenticatedAccountId ||
            data.Inventory.CharacterId != character.CharacterId || data.Presets.PlayerId != character.CharacterId ||
            data.Equipment.Key != character.CharacterId || data.Presets.Records.size() > MaxPresetRecords)
        { throw std::invalid_argument("Menu initialization has inconsistent ownership, scene or presets."); }

        auto profile = Characters::CharacterProfile::BindKnownFields(std::move(data.Profile), character, authenticatedAccountId);
        // Local menu policy: one authenticated player at slot zero. Both S117
        // maps and S806 are derived from the very same supplied equipment state.
        std::array<Scenes::SceneSlot, 1> slots;
        slots[0].State = std::move(data.Member);
        slots[0].Character = character;
        slots[0].Equipment = data.Equipment;
        auto scene = Scenes::SceneRoster::Compose(std::move(data.Scene), slots, 0, authenticatedAccountId);
        auto equipment = Scenes::SceneRoster::CreateInventoryView(authenticatedAccountId, character, data.Equipment);
        auto inventory = data.Inventory.CreateNotification(authenticatedAccountId, observedAtUnixSeconds);

        // S3 clears inventory; S117 establishes the shared context and view
        // indexes; S837 looks up the player's view. This is the local ordered
        // initialization policy, not a claimed original packet capture.
        std::vector<Protocol::TcpPacket> packets;
        packets.reserve(5);
        packets.push_back(profile.Encode());
        packets.push_back(scene.Encode());
        packets.push_back(equipment.Encode());
        packets.push_back(data.Presets.Encode());
        packets.push_back(inventory.Encode());
        return packets;
    }
}
