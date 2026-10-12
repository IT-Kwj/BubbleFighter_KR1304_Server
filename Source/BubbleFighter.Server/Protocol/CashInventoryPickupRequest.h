#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct CashInventoryPickupRequest
    {
        static constexpr std::uint16_t MessageId = 829;
        std::uint32_t SlotId = 0;
        TcpPacket Encode() const;
        static CashInventoryPickupRequest Decode(const TcpPacket& packet);
        bool operator==(const CashInventoryPickupRequest&) const = default;
    };
}
