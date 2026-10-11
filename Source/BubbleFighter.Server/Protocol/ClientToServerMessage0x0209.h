#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E054: empty packet from 0x00937560 / 0x00937920 during common UI
    // initialization. Its original request/response meaning remains unknown.
    struct ClientToServerMessage0x0209
    {
        static constexpr std::uint16_t MessageId = 0x0209;
        TcpPacket Encode() const;
        static ClientToServerMessage0x0209 Decode(const TcpPacket& packet);
    };
}
