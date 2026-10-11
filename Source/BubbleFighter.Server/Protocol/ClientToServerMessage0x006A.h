#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E042/E043: empty packet sent by the plaza button after action gate 2.
    // Other callers exist; complete original server semantics remain unknown.
    struct ClientToServerMessage0x006A
    {
        static constexpr std::uint16_t MessageId = 0x006A;
        TcpPacket Encode() const;
        static ClientToServerMessage0x006A Decode(const TcpPacket& packet);
    };
}
