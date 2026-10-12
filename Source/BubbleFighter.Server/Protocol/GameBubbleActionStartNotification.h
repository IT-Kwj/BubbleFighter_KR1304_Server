#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 6E4600 -> 6D55D0 -> actor +508 (8F3BD0).
    // The first field owns the action; the second is the bubbled target.
    struct GameBubbleActionStartNotification
    {
        static constexpr std::uint16_t MessageId = 0x8017;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t TargetObjectId = 0;
        std::uint32_t Variant = 0;
        TcpPacket Encode() const;
        static GameBubbleActionStartNotification Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleActionStartNotification&) const = default;
    };
}
