#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 8F2500 -> 6E2EC0; 6D55D0 requests state 115 and 4AE3F0.
    // Distinct from 8018, which requests state 116.
    struct GameBubbleActionCompleteNotification
    {
        static constexpr std::uint16_t MessageId = 0x8045;
        std::uint32_t ActorObjectId = 0;
        TcpPacket Encode() const;
        static GameBubbleActionCompleteNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleActionCompleteNotification&) const = default;
    };
}
