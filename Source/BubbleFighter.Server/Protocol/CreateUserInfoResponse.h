#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E028: gspRepCreateUserInfo, read at BFCLIENT02 0x009A46A0.
    struct CreateUserInfoResponse
    {
        static constexpr std::uint16_t MessageId = 0x000E;

        std::uint32_t Result = 0;

        TcpPacket Encode() const;
        static CreateUserInfoResponse Decode(const TcpPacket& packet);
        bool operator==(const CreateUserInfoResponse&) const = default;
    };
}
