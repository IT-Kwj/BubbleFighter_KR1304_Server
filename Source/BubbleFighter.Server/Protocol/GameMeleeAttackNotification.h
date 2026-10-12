#pragma once
#include "TcpPacketCodec.h"
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 8ECB00 -> 6E2990, reader 6F2700; 19 + 4*n bytes.
    struct GameMeleeAttackNotification
    {
        static constexpr std::uint16_t MessageId = 0x81FE;
        static constexpr std::size_t MaxValues = (TcpPacketCodec::DefaultMaxPayloadSize - 19) / 4;
        std::uint32_t ActorObjectId = 0;
        std::uint16_t WeaponId = 0;
        // 8ECB00 obtains this from actor+548, not the selected weapon at+872.
        // Preserve the native byte until its complete semantics are confirmed.
        std::uint8_t ByteAt6 = 0;
        std::uint32_t ShotSequence = 0;
        std::uint32_t PackedTime = 0;
        std::vector<std::uint32_t> Values;
        TcpPacket Encode() const;
        static GameMeleeAttackNotification Decode(const TcpPacket& packet);
        bool operator==(const GameMeleeAttackNotification&) const = default;
    };
}
