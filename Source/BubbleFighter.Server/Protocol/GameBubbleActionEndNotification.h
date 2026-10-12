#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 8F0EF0 writer; 6F7600 requests actor state 116.
    struct GameBubbleActionEndNotification
    {
        static constexpr std::uint16_t MessageId = 0x8018;
        std::uint32_t ActorObjectId = 0;
        TcpPacket Encode() const;
        static GameBubbleActionEndNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleActionEndNotification&) const = default;
    };
}
