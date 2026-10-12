#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 writer/reader evidence: Docs/Protocol/ActorActions.md.
    // Neutral name until the full game action semantics are established.
    struct ClientToServerMessage0x800E
    {
        static constexpr std::uint16_t MessageId = 0x800E;
        std::uint32_t ActorObjectId = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x800E Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x800E&) const = default;
    };
}

