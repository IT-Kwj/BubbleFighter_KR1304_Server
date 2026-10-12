#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomBubble: 6E43D0 / 6FB680; mode 1 emits FFFFFFFF at offset 20.
    struct GameBubbleDeflectNotification
    {
        static constexpr std::uint16_t MessageId = 0x9584;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t SourceObjectId = 0;
        std::array<std::uint32_t, 3> VectorBits{};
        std::uint32_t ValueAt20 = 0;
        std::uint32_t ContactValue = 0;
        TcpPacket Encode() const;
        static GameBubbleDeflectNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleDeflectNotification&) const = default;
    };
}
