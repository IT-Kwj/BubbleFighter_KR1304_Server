#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 writer/reader evidence: Docs/Protocol/ActorActions.md.
    // Neutral name until the full game action semantics are established.
    struct ClientToServerMessage0x800C
    {
        static constexpr std::uint16_t MessageId = 0x800C;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t ValueAt4 = 0;
        std::uint8_t AxisAt8 = 0;
        std::uint8_t AxisAt9 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x800C Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x800C&) const = default;
    };
}

