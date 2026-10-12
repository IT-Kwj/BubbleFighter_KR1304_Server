#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E082: 0x006E2CA0 writes the actor ID; 0x006D8743 reads it and
    // applies Character state 402. Shared wire form in both directions.
    struct GameActorDeathNotification
    {
        static constexpr std::uint16_t MessageId = 0x9582;
        std::uint32_t ActorObjectId = 0;

        TcpPacket Encode() const;
        static GameActorDeathNotification Decode(const TcpPacket& packet);
        bool operator==(const GameActorDeathNotification&) const = default;
    };
}
