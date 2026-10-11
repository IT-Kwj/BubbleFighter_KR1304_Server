#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E043: uint32; nonzero displays the party-moved-to-lobby notice.
    // Both branches request stage 3 when the main connection is ready.
    struct GotoLobbyNotification
    {
        static constexpr std::uint16_t MessageId = 0x0074;
        std::uint32_t PartyMoveNotice = 0;
        TcpPacket Encode() const;
        static GotoLobbyNotification Decode(const TcpPacket& packet);
        bool operator==(const GotoLobbyNotification&) const = default;
    };
}
