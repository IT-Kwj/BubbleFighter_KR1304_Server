#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E076: S4 dispatches gspReqPing. The verified prefix needs no payload;
    // the minimal empty request is local policy, with its protected tail unverified.
    struct ServerPingRequest
    {
        static constexpr std::uint16_t MessageId = 4;
        TcpPacket Encode() const;
        static ServerPingRequest Decode(const TcpPacket& packet);
    };
}
