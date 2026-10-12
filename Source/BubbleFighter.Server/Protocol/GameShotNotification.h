#pragma once
#include "CompressedShotRecord.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct GameShotNotification
    {
        static constexpr std::uint16_t MessageId = 0x809D;
        CompressedShotRecord Shot;
        TcpPacket Encode() const;
        static GameShotNotification Decode(const TcpPacket& packet);
        bool operator==(const GameShotNotification&) const = default;
    };
}
