#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 85CCCB / 6D97D0; offset 8 preserves the native float bit pattern.
    struct GameBubbleRescueNotification
    {
        static constexpr std::uint16_t MessageId = 0x9581;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t HelperObjectId = 0;
        std::uint32_t ValueAt8 = 0;
        std::uint8_t Kind = 0;
        TcpPacket Encode() const;
        static GameBubbleRescueNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleRescueNotification&) const = default;
    };
}
