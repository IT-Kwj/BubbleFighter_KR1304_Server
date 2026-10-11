#pragma once

#include "TcpPacketCodec.h"

#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E028: name-check UI action at BFCLIENT02 0x005CCC60.
    struct CheckCreateUserNameRequest
    {
        static constexpr std::uint16_t MessageId = 0x001F;

        std::u16string CharacterName;

        TcpPacket Encode() const;
        static CheckCreateUserNameRequest Decode(const TcpPacket& packet);
        bool operator==(const CheckCreateUserNameRequest&) const = default;
    };
}
