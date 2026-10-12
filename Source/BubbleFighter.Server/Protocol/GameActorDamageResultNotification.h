#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // RoomDamageRelay: 6E3B80 -> 6F8280, emitted after HP reaches zero. The consumer
    // selects bubble or direct death. This codec alone grants no game state.
    struct GameActorDamageResultNotification
    {
        static constexpr std::uint16_t MessageId = 0x958A;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t SourceObjectId = 0;
        // Three float32 bit patterns. Impact direction/position distinction
        // depends on the damager branch, so preserve the original vector.
        std::array<std::uint32_t, 3> VectorBits{};
        std::uint32_t WeaponId = 0;
        std::uint32_t DirectDeathValue = 0;
        std::uint32_t FullHealthHitValue = 0;
        // Writer replaces the low 24 bits with elapsed time; the upper byte
        // retains stack data. The reader consumes all four bytes.
        std::uint32_t TimeValue = 0;
        std::uint32_t ValueAt36 = 0;
        TcpPacket Encode() const;
        static GameActorDamageResultNotification Decode(const TcpPacket& packet);
        bool operator==(const GameActorDamageResultNotification&) const = default;
    };
}
