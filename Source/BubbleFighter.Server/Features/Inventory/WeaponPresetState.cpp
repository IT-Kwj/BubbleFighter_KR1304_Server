#include "WeaponPresetState.h"
#include "ItemCatalog.h"
#include "Protocol/InventoryViewUpdateNotification.h"
#include "Protocol/WeaponPresetChangeResponse.h"
#include <algorithm>
#include <stdexcept>

namespace BubbleFighter::Server::Features::Inventory
{
    namespace
    {
        constexpr std::uint32_t PrimaryMask = std::uint32_t{1} << 18; // E059 sWeapon1
        bool IsPrimary(std::uint32_t id)
        {
            const auto* item = ItemCatalog::Find(id);
            return id > 200000 && id < 300000 && item && item->SlotMask == PrimaryMask && item->Function == 0;
        }
    }

    void WeaponPresetState::Validate() const
    {
        if (PrimaryItems.empty() || PrimaryItems.size() > Protocol::WeaponPresetChangeRequest::MaxPresetIndex + 1U ||
            ActiveIndex >= PrimaryItems.size())
        { throw std::invalid_argument("Invalid owned weapon preset pages."); }
        for (const auto id : PrimaryItems)
        {
            if (id != 0 && !IsPrimary(id)) { throw std::invalid_argument("Unsupported primary weapon preset item."); }
        }
    }

    void WeaponPresetState::CaptureActive(const CharacterInventory& inventory)
    {
        Validate();
        std::uint32_t primary = 0;
        for (const auto& slot : inventory.Slots)
        {
            if (!slot.Equipped || !IsPrimary(slot.ItemId)) { continue; }
            if (primary != 0) { throw std::invalid_argument("Conflicting active primary weapons."); }
            primary = slot.ItemId;
        }
        PrimaryItems[ActiveIndex] = primary;
    }

    Protocol::WeaponPresetUpdateNotification WeaponPresetState::CreateNotification(
        const Characters::CharacterRecord& character, const CharacterInventory& inventory, std::uint64_t observedAtUnixSeconds) const
    {
        Validate();
        if (inventory.AccountId != character.AccountId || inventory.CharacterId != character.CharacterId ||
            character.CharacterType >= 32 || !character.Statistics.IsValid())
        { throw std::invalid_argument("Invalid weapon preset owner or character."); }
        const auto records = inventory.ToRecords(character.AccountId, observedAtUnixSeconds);
        Protocol::WeaponPresetUpdateNotification result;
        result.ByteAt0 = ActiveIndex;
        result.PlayerId = static_cast<std::uint32_t>(character.CharacterId);
        for (auto id : PrimaryItems)
        {
            if (id != 0)
            {
                const auto slot = std::find_if(inventory.Slots.begin(), inventory.Slots.end(),
                    [&](const auto& item) { return item.ItemId == id; });
                const auto* definition = ItemCatalog::Find(id);
                if (slot == inventory.Slots.end()) { throw std::invalid_argument("Preset weapon is not owned."); }
                if (slot->Usage != definition->Usage) { throw std::invalid_argument("Preset weapon usage mismatch."); }
                // Expired/depleted/ineligible saved weapons become empty for
                // display and selection, not renewed, replaced or re-granted.
                const auto index = static_cast<std::size_t>(slot - inventory.Slots.begin());
                if (!definition->Enabled || (records[index].Flags & 2) != 0 ||
                    character.Statistics.Level < definition->RequiredLevel ||
                    (definition->AllowedCharacters & (std::uint32_t{1} << character.CharacterType)) == 0)
                { id = 0; }
            }
            // E066 only the first member is confirmed as the primary weapon.
            // A29090 accepts zero as an empty preview. Other positions and the
            // trailing byte vector stay explicitly empty in this local subset.
            result.Records.push_back({id, 0, 0});
        }
        return result;
    }

    WeaponPresetChangeResult PrepareWeaponPresetChange(const Characters::CharacterRecord& character,
        CharacterInventory inventory, WeaponPresetState presets, const Protocol::WeaponPresetChangeRequest& request,
        std::uint64_t observedAtUnixSeconds)
    {
        (void)request.Encode();
        presets.CaptureActive(inventory);
        if (request.ObjectId != Protocol::WeaponPresetChangeRequest::MenuObjectId || request.PresetIndex >= presets.PrimaryItems.size())
        { throw std::invalid_argument("Weapon preset is not available in the menu/plaza."); }
        const auto available = presets.CreateNotification(character, inventory, observedAtUnixSeconds);
        // Persist cleanup of unavailable selections with the equipment change.
        for (std::size_t i = 0; i != available.Records.size(); ++i) { presets.PrimaryItems[i] = available.Records[i][0]; }
        presets.ActiveIndex = request.PresetIndex;
        const auto selected = presets.PrimaryItems[presets.ActiveIndex];
        for (auto& slot : inventory.Slots)
        {
            if (IsPrimary(slot.ItemId)) { slot.Equipped = slot.ItemId == selected; }
        }
        const auto view = EquipmentView::Create(character, inventory, observedAtUnixSeconds);
        std::vector<Protocol::TcpPacket> packets;
        packets.push_back(inventory.CreateNotification(character.AccountId, observedAtUnixSeconds).Encode());
        packets.push_back(Protocol::InventoryViewUpdateNotification{view}.Encode());
        packets.push_back(presets.CreateNotification(character, inventory, observedAtUnixSeconds).Encode());
        packets.push_back(Protocol::WeaponPresetChangeResponse{request.PresetIndex, Protocol::WeaponPresetChangeResponse::Success}.Encode());
        return {std::move(inventory), std::move(presets), std::move(packets)};
    }
}
