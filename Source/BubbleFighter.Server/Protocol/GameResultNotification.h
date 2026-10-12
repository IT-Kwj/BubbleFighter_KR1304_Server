#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E072: 0x009E308C reads 4+4+1+1 bytes; 0x009E9710 enters Result.
    // Header/counter meaning remains unknown. Codec only, not sent by practice.
    struct GameResultNotification
    {
        static constexpr std::uint16_t MessageId = 613;
        std::uint32_t ValueAt0 = 0;
        std::uint32_t ElapsedMilliseconds = 0; // 0x00D9FDB0 divides by 1000 for Match_RoundEnd.
        std::uint8_t ByteAt8 = 0;
        std::uint8_t ByteAt9 = 0;
        TcpPacket Encode() const;
        static GameResultNotification Decode(const TcpPacket& packet);
        bool operator==(const GameResultNotification&) const = default;
    };
}
