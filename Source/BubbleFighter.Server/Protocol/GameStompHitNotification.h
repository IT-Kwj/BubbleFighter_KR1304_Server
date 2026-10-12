#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 8E1E10 state 117 -> 6F7780 -> 4A8D30/4A91F0.
    struct GameStompHitNotification
    {
        static constexpr std::uint16_t MessageId = 0x8030;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t TargetObjectId = 0;
        TcpPacket Encode() const;
        static GameStompHitNotification Decode(const TcpPacket& packet);
        bool operator==(const GameStompHitNotification&) const = default;
    };
}
