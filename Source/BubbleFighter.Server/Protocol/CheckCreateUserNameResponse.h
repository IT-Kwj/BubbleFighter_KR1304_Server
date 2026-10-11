#pragma once

#include "TcpPacketCodec.h"

#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E028: gspRepCheckCreateUserName, read at BFCLIENT02 0x009A4E90.
    struct CheckCreateUserNameResponse
    {
        static constexpr std::uint16_t MessageId = 0x000D;

        std::uint32_t Result = 0;
        // Present even on failure; on acceptance this is used by the create request.
        std::u16string CharacterName;

        TcpPacket Encode() const;
        static CheckCreateUserNameResponse Decode(const TcpPacket& packet);
        bool operator==(const CheckCreateUserNameResponse&) const = default;
    };
}
