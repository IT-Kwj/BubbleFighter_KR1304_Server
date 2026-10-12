#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 6E32D0 -> 6F65A0. Unlike the reference server's sample,
    // this sample writes six DWORDs, including the final byte widened to DWORD.
    struct GameWeaponSwitchNotification
    {
        static constexpr std::uint16_t MessageId = 0x802A;
        std::uint32_t WeaponId = 0;
        std::uint32_t WeaponIndex = 0;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t TimeValue = 0;
        std::uint32_t SelectValue = 0;
        std::uint32_t ValueAt20 = 0;
        TcpPacket Encode() const;
        static GameWeaponSwitchNotification Decode(const TcpPacket& packet);
        bool operator==(const GameWeaponSwitchNotification&) const = default;
    };
}
