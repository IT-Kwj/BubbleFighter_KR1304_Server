#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 writer/reader evidence: Docs/Protocol/ActorActions.md.
    // Neutral name until the full game action semantics are established.
    struct ClientToServerMessage0x800D
    {
        static constexpr std::uint16_t MessageId = 0x800D;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t ValueBitsAt4 = 0;
        std::uint32_t ValueAt8 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x800D Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x800D&) const = default;
    };
}

