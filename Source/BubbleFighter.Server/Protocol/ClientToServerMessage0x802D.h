#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 writer/reader evidence: Docs/Protocol/ActorActions.md.
    // Neutral name until the full game action semantics are established.
    struct ClientToServerMessage0x802D
    {
        static constexpr std::uint16_t MessageId = 0x802D;
        std::uint32_t ActorObjectId = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x802D Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x802D&) const = default;
    };
}

