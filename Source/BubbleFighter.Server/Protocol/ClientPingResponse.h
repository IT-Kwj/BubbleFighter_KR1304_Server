#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E076: original protected S4 handler constructs C41 with no payload.
    struct ClientPingResponse
    {
        static constexpr std::uint16_t MessageId = 41;
        TcpPacket Encode() const;
        static ClientPingResponse Decode(const TcpPacket& packet);
    };
}
