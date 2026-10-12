#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 706840 writer / 6F4EF0 reader, 24 bytes.
    struct GameWeaponImpactNotification
    {
        static constexpr std::uint16_t MessageId = 0x8215;
        std::uint32_t DamagerKey = 0;
        std::array<std::uint32_t, 3> PositionBits{};
        // 7966C0 copies GoDamager+656; +692 supplies DamagerKey.
        std::uint32_t SourceObjectId = 0;
        std::uint32_t TargetObjectId = 0;
        TcpPacket Encode() const;
        static GameWeaponImpactNotification Decode(const TcpPacket& packet);
        bool operator==(const GameWeaponImpactNotification&) const = default;
    };
}
