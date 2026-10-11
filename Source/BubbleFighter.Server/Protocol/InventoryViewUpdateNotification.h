#pragma once

#include "UpdatePartyNotification.h"

namespace BubbleFighter::Server::Protocol
{
    // E049: S806 gspInventoryViewUpdateView, reader 0x009AF2F0.
    // The leading key is compared with the local player identity. The rest
    // uses the same supplementary record as S201/S117; meanings remain partial.
    struct InventoryViewUpdateNotification
    {
        static constexpr std::uint16_t MessageId = 0x0326;
        PartySupplementaryRecord View;

        TcpPacket Encode() const;
        static InventoryViewUpdateNotification Decode(const TcpPacket& packet);
        bool operator==(const InventoryViewUpdateNotification&) const = default;
    };
}
