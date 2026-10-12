#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 4AAFA7 / 6D97D0; helper precedes victim, unlike the request.
    struct GameBubbleRescueEffectNotification
    {
        static constexpr std::uint16_t MessageId = 0x9588;
        std::uint32_t HelperObjectId = 0;
        std::uint32_t ActorObjectId = 0;
        std::uint8_t Kind = 0;
        TcpPacket Encode() const;
        static GameBubbleRescueEffectNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleRescueEffectNotification&) const = default;
    };
}
