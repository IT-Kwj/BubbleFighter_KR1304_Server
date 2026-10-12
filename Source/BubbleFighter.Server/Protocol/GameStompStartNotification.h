#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 492E70 -> 6E4730; 6D55D0 -> 4A8BA0 (state 117).
    struct GameStompStartNotification
    {
        static constexpr std::uint16_t MessageId = 0x802F;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t TargetObjectId = 0;
        TcpPacket Encode() const;
        static GameStompStartNotification Decode(const TcpPacket& packet);
        bool operator==(const GameStompStartNotification&) const = default;
    };
}
