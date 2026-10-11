#pragma once

#include "MySqlConnection.h"
#include "Features/Inventory/InventoryStore.h"

namespace BubbleFighter::Server::Persistence
{
    class MySqlInventoryStore final : public Features::Inventory::InventoryStore
    {
    public:
        explicit MySqlInventoryStore(const std::filesystem::path& configuration);
        std::optional<Features::Inventory::CharacterInventory> Load(std::uint64_t authenticatedAccountId,
            std::uint64_t characterId, std::uint64_t observedAtUnixSeconds) override;
        bool Save(std::uint64_t authenticatedAccountId, std::uint64_t characterId,
            Features::Inventory::InventorySlot& slot, std::uint64_t observedAtUnixSeconds) override;
        Features::Inventory::EquipmentChangeResult ChangeEquipment(const Features::Characters::CharacterRecord& character,
            std::uint64_t itemIdentifier, bool equip, std::uint64_t observedAtUnixSeconds) override;

    private:
        MySqlConnection connection_;
    };
}
