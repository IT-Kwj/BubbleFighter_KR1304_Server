#pragma once

#include "MySqlConnection.h"
#include "Features/Inventory/InventoryStore.h"
#include "Features/Shop/ShopPurchase.h"
#include "Features/Shop/CashShop.h"
#include "Features/Inventory/WeaponPresetState.h"
#include "Features/Inventory/ItemUse.h"

namespace BubbleFighter::Server::Persistence
{
    struct InventorySnapshot
    {
        Features::Inventory::CharacterInventory Inventory;
        Features::Inventory::CashInventory Cash;
        Features::Inventory::WeaponPresetState Presets;
    };

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
        Features::Shop::ShopPurchaseResult Purchase(const Features::Characters::CharacterRecord& character,
            const Protocol::ShopPurchaseRequest& request, std::uint64_t observedAtUnixSeconds);

        std::optional<InventorySnapshot> LoadSnapshot(std::uint64_t authenticatedAccountId,
            std::uint64_t characterId, std::uint64_t observedAtUnixSeconds);
        // Atomically fills missing starter weapons/current character before the first
        // menu/refresh response. Also repairs roles created by older versions.
        std::optional<InventorySnapshot> LoadMenuSnapshot(const Features::Characters::CharacterRecord& character,
            std::uint64_t observedAtUnixSeconds);
        Features::Shop::CashPickupResult Pickup(const Features::Characters::CharacterRecord& character,
            const Protocol::CashInventoryPickupRequest& request, std::uint64_t observedAtUnixSeconds);
        Features::Inventory::WeaponPresetChangeResult ChangeWeaponPreset(const Features::Characters::CharacterRecord& character,
            const Protocol::WeaponPresetChangeRequest& request, std::uint64_t observedAtUnixSeconds);
        Features::Inventory::ItemUseResult UseItem(const Features::Characters::CharacterRecord& character,
            const Protocol::ItemUseRequest& request, std::uint64_t observedAtUnixSeconds);

    private:
        MySqlConnection connection_;
    };
}
