#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 6E2FD0 / 6F7B30; self exit uses source FFFFFFFF or FFFFFFFE.
    struct GameBubbleExitNotification
    {
        static constexpr std::uint16_t MessageId = 0x9580;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t SourceObjectId = 0;
        TcpPacket Encode() const;
        static GameBubbleExitNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleExitNotification&) const = default;
    };
}
