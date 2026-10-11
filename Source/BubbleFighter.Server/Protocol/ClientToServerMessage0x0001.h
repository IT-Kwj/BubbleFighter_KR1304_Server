#pragma once

#include "TcpPacketCodec.h"

#include <array>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    struct ClientToServerMessage0x0001
    {
        static constexpr std::uint16_t MessageId = 0x0001;
        std::uint32_t InitialValue = 0;
        std::array<std::u16string, 22> StringFields;
        std::uint32_t IntermediateValue = 0;

        TcpPacket Encode() const;
        static ClientToServerMessage0x0001 Decode(const TcpPacket& packet);
    };
}
