#pragma once
#include "TcpPacketCodec.h"
#include <array>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 6DF330 -> 708820, reader 6CAB40: 52 + 4*n bytes.
    struct GameGrenadeDetonateNotification
    {
        static constexpr std::uint16_t MessageId = 0x8084;
        static constexpr std::size_t MaxValues = (TcpPacketCodec::DefaultMaxPayloadSize - 52) / 4;
        std::uint32_t SourceDamagerKey = 0;
        std::array<std::uint32_t, 3> PositionBits{};
        // Two further native vectors; preserve their roles without guessing.
        std::array<std::uint32_t, 6> VectorBits{};
        std::uint32_t DamagerKey = 0;
        std::uint32_t PackedTime = 0;
        std::vector<std::uint32_t> Values;
        TcpPacket Encode() const;
        static GameGrenadeDetonateNotification Decode(const TcpPacket& packet);
        bool operator==(const GameGrenadeDetonateNotification&) const = default;
    };
}
