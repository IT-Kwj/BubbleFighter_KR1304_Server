#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E083: 0x006E2DB0 writes 0x8044; 0x006F6900 invokes the actor's
    // +572 callback, which selects state 401 and starts its respawn wait.
    struct GameActorRespawnWaitNotification
    {
        static constexpr std::uint16_t MessageId = 0x8044;
        std::uint32_t ActorObjectId = 0;
        TcpPacket Encode() const;
        static GameActorRespawnWaitNotification Decode(const TcpPacket& packet);
        bool operator==(const GameActorRespawnWaitNotification&) const = default;
    };
}
