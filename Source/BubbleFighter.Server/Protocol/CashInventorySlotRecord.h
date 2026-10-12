#pragma once

#include <cstdint>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 CashInvenSlot, native size 52. Wire size is variable;
    // no vtable or native string object is transmitted.
    struct CashInventorySlotRecord
    {
        std::uint32_t SlotId = 0;             // native +4, pickup/map key
        std::uint32_t ValueAt8 = 0;           // unresolved
        std::uint16_t Quantity = 0;          // native +12
        std::uint16_t DurationDays = 0;      // native +14; >10000 displays permanent
        std::uint32_t StockId = 0;           // native +16, purchase/result dialog
        std::uint32_t ItemId = 0;            // native +20, ordinary item resource lookup
        std::u16string TextAt24;             // unresolved
        std::uint8_t ByteAt48 = 0;           // 1 exposes cancellation UI
        bool operator==(const CashInventorySlotRecord&) const = default;
    };
}
