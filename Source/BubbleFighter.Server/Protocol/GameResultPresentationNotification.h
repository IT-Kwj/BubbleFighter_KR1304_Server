#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E072: 0x009E323D reads one byte and passes bit 0 to 0x009E9EF0.
    // Preserve raw flags; only the consumer of bit 0 has been identified.
    struct GameResultPresentationNotification
    {
        static constexpr std::uint16_t MessageId = 615;
        std::uint8_t Flags = 0;
        TcpPacket Encode() const;
        static GameResultPresentationNotification Decode(const TcpPacket& packet);
        bool operator==(const GameResultPresentationNotification&) const = default;
    };
}
