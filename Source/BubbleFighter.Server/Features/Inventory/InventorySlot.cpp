#include "InventorySlot.h"

#include <bit>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace BubbleFighter::Server::Features::Inventory
{
    Protocol::InventoryItemRecord InventorySlot::ToProtocol(std::uint64_t observedAtUnixSeconds) const
    {
        if (ItemId == 0 || ItemId > MaxItemId || Quantity > MaxQuantity ||
            observedAtUnixSeconds > MaxUnixSeconds ||
            (Usage != InventoryUsage::Count && Usage != InventoryUsage::Duration && Usage != InventoryUsage::Life) ||
            (ExpiresAtUnixSeconds && (Usage != InventoryUsage::Duration || *ExpiresAtUnixSeconds > MaxUnixSeconds)))
        { throw std::invalid_argument("Invalid ordinary inventory slot."); }

        double remainingDays = 0.0;
        bool exhausted = Quantity == 0;
        if (Usage == InventoryUsage::Duration)
        {
            exhausted = ExpiresAtUnixSeconds && *ExpiresAtUnixSeconds <= observedAtUnixSeconds;
            if (!ExpiresAtUnixSeconds) { remainingDays = PermanentDisplayDays; }
            else if (!exhausted)
            {
                const auto seconds = *ExpiresAtUnixSeconds - observedAtUnixSeconds;
                if (seconds > MaxFiniteSeconds)
                { throw std::invalid_argument("Finite inventory duration exceeds the local display limit."); }
                remainingDays = static_cast<double>(seconds) / 86'400.0;
            }
        }

        Protocol::InventoryItemRecord record;
        record.Key = ItemId;
        record.ValueAt4 = Quantity;
        static_assert(sizeof(double) == sizeof(std::uint64_t) && std::numeric_limits<double>::is_iec559);
        record.BitsAt8 = std::bit_cast<std::uint64_t>(remainingDays);
        record.TextAt36 = TextAt36;
        record.AttributesAt24 = AttributesAt24;
        // Ordinary slots only: bits 0/1 are equipped and depleted/expired.
        // Premium and other special flags are intentionally outside this model.
        record.Flags = static_cast<std::uint8_t>((Equipped ? 1 : 0) | (exhausted ? 2 : 0));
        return record;
    }

    std::vector<Protocol::InventoryItemRecord> CharacterInventory::ToRecords(
        std::uint64_t authenticatedAccountId, std::uint64_t observedAtUnixSeconds) const
    {
        if (AccountId == 0 || AccountId != authenticatedAccountId || CharacterId == 0 ||
            CharacterId > 0xFFFFFFFFULL || observedAtUnixSeconds > InventorySlot::MaxUnixSeconds ||
            Slots.size() > Protocol::InventoryItemRecord::MaxCollectionEntries)
        { throw std::invalid_argument("Invalid inventory identity or size."); }
        std::unordered_set<std::uint32_t> ids;
        std::vector<Protocol::InventoryItemRecord> records;
        records.reserve(Slots.size());
        for (const auto& slot : Slots)
        {
            if (!ids.insert(slot.ItemId).second) { throw std::invalid_argument("Duplicate inventory item ID."); }
            records.push_back(slot.ToProtocol(observedAtUnixSeconds));
        }
        // Enforce the cumulative attribute/string/byte budgets, not just each
        // row's validity. A real empty stored inventory is represented by zero.
        (void)Protocol::InventoryItemRecord::EncodeList(records);
        return records;
    }

    Protocol::InventoryUpdateAllNotification CharacterInventory::CreateNotification(
        std::uint64_t authenticatedAccountId, std::uint64_t observedAtUnixSeconds) const
    {
        return Protocol::InventoryUpdateAllNotification::FromRecords(
            ToRecords(authenticatedAccountId, observedAtUnixSeconds));
    }
}
