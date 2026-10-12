#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E069: 0x009E1EB0 case 608 -> 0x009E7570, GameStart event and timer.
    // E071: engine+2152 -> 0x006D1EA0 -> 0x00F99280 RNG initialization.
    // Also echoed by C103; it is not a unique match ID or an authorization token.
    struct GameStartNotification
    {
        static constexpr std::uint16_t MessageId = 608;
        std::uint32_t DurationMilliseconds = 0;
        std::uint32_t RandomSeed = 0;

        TcpPacket Encode() const;
        static GameStartNotification Decode(const TcpPacket& packet);
        bool operator==(const GameStartNotification&) const = default;
    };
}
