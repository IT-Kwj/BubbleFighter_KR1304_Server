#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 6E40C0 / 6D97D0; kind is a byte, not a boolean.
    struct GameBubbleRescueRequest
    {
        static constexpr std::uint16_t MessageId = 0x8200;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t HelperObjectId = 0;
        std::uint8_t Kind = 0;
        TcpPacket Encode() const;
        static GameBubbleRescueRequest Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleRescueRequest&) const = default;
    };
}
