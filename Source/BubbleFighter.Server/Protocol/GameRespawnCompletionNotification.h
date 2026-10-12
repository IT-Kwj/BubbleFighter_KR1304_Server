#pragma once
#include "RespawnParameters.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E080: client-generated 0x809E passed to routing helper 0x007AB190,
    // also consumed by the game dispatcher. Its actual route is state-dependent.
    struct GameRespawnCompletionNotification
    {
        static constexpr std::uint16_t MessageId = 0x809E;
        RespawnParameters Parameters;
        std::uint8_t ByteAt20 = 0;
        std::uint32_t ValueAt21 = 0;
        TcpPacket Encode() const;
        static GameRespawnCompletionNotification Decode(const TcpPacket& packet);
        bool operator==(const GameRespawnCompletionNotification&) const = default;
    };
}
