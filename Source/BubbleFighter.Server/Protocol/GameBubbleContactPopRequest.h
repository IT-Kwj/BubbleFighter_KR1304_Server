#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 8EE685 writer / 6FA9A0; second actor is the contact initiator.
    struct GameBubbleContactPopRequest
    {
        static constexpr std::uint16_t MessageId = 0x9583;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t SourceObjectId = 0;
        TcpPacket Encode() const;
        static GameBubbleContactPopRequest Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleContactPopRequest&) const = default;
    };
}
