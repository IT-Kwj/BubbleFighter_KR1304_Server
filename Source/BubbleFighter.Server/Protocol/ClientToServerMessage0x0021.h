#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E028: empty follow-up sent from the create response handler.
    // The send site alone does not establish the server-side operation.
    struct ClientToServerMessage0x0021
    {
        static constexpr std::uint16_t MessageId = 0x0021;

        TcpPacket Encode() const;
        static ClientToServerMessage0x0021 Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x0021&) const = default;
    };
}
