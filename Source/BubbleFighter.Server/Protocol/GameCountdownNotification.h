#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E069: 0x009E1EB0 case 607 -> 0x009E7080 sets the pregame timer.
    // This is a wire value, not an approved countdown policy.
    struct GameCountdownNotification
    {
        static constexpr std::uint16_t MessageId = 607;
        std::uint32_t DurationMilliseconds = 0;

        TcpPacket Encode() const;
        static GameCountdownNotification Decode(const TcpPacket& packet);
        bool operator==(const GameCountdownNotification&) const = default;
    };
}
