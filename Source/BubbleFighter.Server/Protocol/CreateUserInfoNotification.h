#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E031: gspNotifyCreateUserInfo, handled without reading a payload.
    struct CreateUserInfoNotification
    {
        static constexpr std::uint16_t MessageId = 0x000C;
        TcpPacket Encode() const;
        static CreateUserInfoNotification Decode(const TcpPacket& packet);
    };
}
