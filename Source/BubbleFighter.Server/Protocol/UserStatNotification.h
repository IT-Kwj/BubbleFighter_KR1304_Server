#pragma once

#include "FighterInfo.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E026: gspUserStat, original outer reader at 0x009D8030.
    struct UserStatNotification
    {
        static constexpr std::uint16_t MessageId = 0x0069;

        // E051: [0] color, [1] experience, [4] Lucci, [5] Cash.
        // RoomSnapshot: [2]/[3] experience range base/next, supplied by the
        // configured local policy. [6]/[7] remain explicit/unknown.
        std::array<std::uint32_t, 8> Values{};
        std::u16string Text;
        std::uint16_t ShortValue = 0;
        FighterInfo Fighter;

        TcpPacket Encode() const;
        static UserStatNotification Decode(const TcpPacket& packet);
        bool operator==(const UserStatNotification&) const = default;
    };
}
