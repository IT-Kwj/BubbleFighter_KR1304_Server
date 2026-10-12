#pragma once

#include "CashInventorySlotRecord.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct CashInventoryUpdateNotification
    {
        static constexpr std::uint16_t MessageId = 810;
        // Local allocation policies, not recovered original-server limits.
        static constexpr std::size_t MaxSlots = 1024;
        static constexpr std::size_t MaxPayloadBytes = 262144;
        std::vector<CashInventorySlotRecord> Slots;
        TcpPacket Encode() const;
        static CashInventoryUpdateNotification Decode(const TcpPacket& packet);
        bool operator==(const CashInventoryUpdateNotification&) const = default;
    };
}
