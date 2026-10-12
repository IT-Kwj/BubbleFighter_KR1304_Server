#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E071: 0x00708B60 writes uint32 + zero-extended uint8 as uint32.
    // Training btnQuit calls 0x006E5840(1), emitting {1, 0}. Other uses unknown.
    struct ClientToServerMessage0x0270
    {
        static constexpr std::uint16_t MessageId = 624;
        std::uint32_t ValueAt0 = 0;
        std::uint32_t ValueAt4 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x0270 Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x0270&) const = default;
    };
}
