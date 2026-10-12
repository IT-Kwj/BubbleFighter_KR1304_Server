#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E072: 0x009E2FE0 / 0x009E88C0, fixed two uint32 fields.
    // Reason 2 selects time-over.png. Other reason values remain unnamed.
    struct GameOverNotification
    {
        static constexpr std::uint16_t MessageId = 612;
        static constexpr std::uint32_t TimeOver = 2;
        std::uint32_t Reason = 0;
        std::uint32_t ValueAt4 = 0;
        TcpPacket Encode() const;
        static GameOverNotification Decode(const TcpPacket& packet);
        bool operator==(const GameOverNotification&) const = default;
    };
}
