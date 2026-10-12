#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 6E3F00 / 6FAC30; flags stay full uint32 on the wire.
    struct GameBubblePopNotification
    {
        static constexpr std::uint16_t MessageId = 0x9585;
        std::uint32_t ApplyValue = 0;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t SourceObjectId = 0;
        std::array<std::uint32_t, 3> VectorBits{};
        std::uint32_t ContactValue = 0;
        std::uint32_t ReasonValue = 0;
        TcpPacket Encode() const;
        static GameBubblePopNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubblePopNotification&) const = default;
    };
}
