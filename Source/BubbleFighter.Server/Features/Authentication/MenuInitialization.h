#pragma once

#include "Features/Characters/CharacterStore.h"
#include "Features/Inventory/InventorySlot.h"
#include "Features/Inventory/InventoryRefresh.h"
#include "Features/Inventory/CashInventory.h"
#include <optional>
#include "Protocol/GotoRoomNotification.h"
#include "Protocol/UserStatNotification.h"
#include "Protocol/WeaponPresetUpdateNotification.h"

namespace BubbleFighter::Server::Features::Authentication
{
    // A trusted data source must provide COMPLETE opaque state. This aggregate
    // is not a zero-filled new-character template. Known identity/statistics
    // fields are bound centrally, while unknown fields remain explicit.
    struct MenuInitializationData
    {
        Protocol::UserStatNotification Profile;
        Protocol::GotoRoomNotification Scene;
        Protocol::SessionMember Member;
        Protocol::PartySupplementaryRecord Equipment;
        Protocol::WeaponPresetUpdateNotification Presets;
        Inventory::CharacterInventory Inventory;
        // Optional only for legacy callers; MySQL always supplies a loaded snapshot.
        std::optional<Inventory::CashInventory> Cash;
    };

    class MenuInitialization
    {
    public:
        // E054: 0x0081B820 narrows preset count to uint8. This is a safety
        // ceiling, not a recovered number of available preset pages.
        static constexpr std::size_t MaxPresetRecords = Inventory::InventoryRefresh::MaxPresetRecords;
        // Builds the ENTIRE ordered batch before handing anything to transport.
        // S3 and the authenticated solo S201 have already been sent.
        static std::vector<Protocol::TcpPacket> Prepare(const Characters::CharacterRecord& character,
            std::uint64_t authenticatedAccountId, MenuInitializationData data,
            std::uint64_t observedAtUnixSeconds);
    };
}
