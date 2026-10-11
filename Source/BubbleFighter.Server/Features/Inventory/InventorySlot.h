#pragma once

#include "Protocol/InventoryUpdateAllNotification.h"

#include <optional>

namespace BubbleFighter::Server::Features::Inventory
{
    // E053: client catalog +208 selects these three display/use branches.
    // The caller must obtain this from a verified catalog, not guess by item ID.
    enum class InventoryUsage : std::uint8_t { Unknown = 0, Count = 1, Duration = 2, Life = 3 };

    struct InventorySlot
    {
        static constexpr std::uint32_t MaxItemId = 99'999'999;
        static constexpr std::uint32_t MaxQuantity = 0x7FFFFFFF;
        static constexpr std::uint64_t MaxUnixSeconds = 253'402'300'799;
        static constexpr std::uint64_t MaxFiniteSeconds = 10'000ULL * 86'400;
        // Local representation, supported by the client's >10000-day display
        // branch. This value is not evidence of the original server's policy.
        static constexpr double PermanentDisplayDays = 36'500.0;

        std::uint32_t ItemId = 0;
        InventoryUsage Usage = InventoryUsage::Unknown;
        std::uint32_t Quantity = 0;
        // UTC Unix seconds; null means permanent only for Duration items.
        std::optional<std::uint64_t> ExpiresAtUnixSeconds;
        bool Equipped = false;
        // Retain unknown detail fields without assigning invented meanings.
        std::u16string TextAt36;
        std::vector<Protocol::InventoryAttributeRecord> AttributesAt24;
        // Local optimistic concurrency version; never serialized to the game.
        std::uint64_t Revision = 0;

        Protocol::InventoryItemRecord ToProtocol(std::uint64_t observedAtUnixSeconds) const;
        bool operator==(const InventorySlot&) const = default;
    };

    struct CharacterInventory
    {
        std::uint64_t AccountId = 0;
        std::uint64_t CharacterId = 0;
        std::vector<InventorySlot> Slots;

        std::vector<Protocol::InventoryItemRecord> ToRecords(
            std::uint64_t authenticatedAccountId, std::uint64_t observedAtUnixSeconds) const;
        Protocol::InventoryUpdateAllNotification CreateNotification(
            std::uint64_t authenticatedAccountId, std::uint64_t observedAtUnixSeconds) const;
    };
}
