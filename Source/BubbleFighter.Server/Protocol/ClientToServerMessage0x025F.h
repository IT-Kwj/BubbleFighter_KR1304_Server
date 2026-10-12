#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E079: 0x0075544A takes the local actor's +448 field; 0x006DDE20
    // writes it as one uint32. It is not an account/character database ID.
    struct ClientToServerMessage0x025F
    {
        static constexpr std::uint16_t MessageId = 607;
        std::uint32_t ValueAt0 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x025F Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x025F&) const = default;
    };
}
