#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct CashBalanceRequest
    {
        static constexpr std::uint16_t MessageId = 828;
        TcpPacket Encode() const;
        static CashBalanceRequest Decode(const TcpPacket& packet);
        bool operator==(const CashBalanceRequest&) const = default;
    };
}
