#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 writer/reader evidence: Docs/Protocol/ActorActions.md.
    // Neutral name until the full game action semantics are established.
    struct ClientToServerMessage0x800B
    {
        static constexpr std::uint16_t MessageId = 0x800B;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t ValueAt4 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x800B Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x800B&) const = default;
    };
}

