#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 6E4D60 -> 6D55D0 -> Character virtual +552/4A6F90.
    // Completes the rolling entry animation and requests native state 301.
    struct GameBubbleSettledNotification
    {
        static constexpr std::uint16_t MessageId = 0x9589;
        std::uint32_t ActorObjectId = 0;
        TcpPacket Encode() const;
        static GameBubbleSettledNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleSettledNotification&) const = default;
    };
}
