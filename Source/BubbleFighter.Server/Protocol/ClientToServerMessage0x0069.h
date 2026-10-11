#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 0x0063A510: empty C105. Observed statically in the stage-3
    // task-bar path; keep direction/number until all callers are understood.
    struct ClientToServerMessage0x0069
    {
        static constexpr std::uint16_t MessageId = 0x0069;
        TcpPacket Encode() const;
        static ClientToServerMessage0x0069 Decode(const TcpPacket& packet);
    };
}
