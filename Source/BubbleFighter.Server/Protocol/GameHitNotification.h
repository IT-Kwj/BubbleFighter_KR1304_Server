#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // E087: 19 bytes, distinct damager lookup key (not ActorObjectId).
    struct GameHitNotification
    {
        static constexpr std::uint16_t MessageId = 0x958B;
        std::uint32_t DamagerKey = 0;
        std::uint32_t TargetObjectId = 0;
        std::array<std::uint16_t, 3> Position{}; // Binary16 bits, preserved.
        std::uint8_t HitKind = 0;
        // Native consumer treats any nonzero uint32 as true. Preserve the
        // received value; the observed sender narrows its input to uint8.
        std::uint32_t KeepDamagerValue = 0;
        TcpPacket Encode() const;
        static GameHitNotification Decode(const TcpPacket& packet);
        bool operator==(const GameHitNotification&) const = default;
    };
}
