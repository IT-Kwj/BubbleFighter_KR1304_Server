#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // E084: complete WanderInfo, 21 wire bytes. At offsets refer to the
    // client's 22-byte record, which has padding before its last uint16.
    struct GameMovementNotification
    {
        static constexpr std::uint16_t MessageId = 0x8075;
        std::uint32_t ActorObjectId = 0;
        std::array<float, 3> Position{};
        std::uint8_t AngleAt16 = 0;
        std::uint8_t AngleAt17 = 0;
        std::uint8_t FlagsAt18 = 0;
        std::uint16_t ValueAt20 = 0;
        TcpPacket Encode() const;
        static GameMovementNotification Decode(const TcpPacket& packet);
        bool operator==(const GameMovementNotification&) const = default;
    };
}
