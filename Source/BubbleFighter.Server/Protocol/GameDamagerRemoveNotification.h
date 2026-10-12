#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct GameDamagerRemoveNotification
    {
        static constexpr std::uint16_t MessageId = 0x808B;
        std::uint32_t DamagerKey = 0;
        TcpPacket Encode() const;
        static GameDamagerRemoveNotification Decode(const TcpPacket& packet);
        bool operator==(const GameDamagerRemoveNotification&) const = default;
    };
}
