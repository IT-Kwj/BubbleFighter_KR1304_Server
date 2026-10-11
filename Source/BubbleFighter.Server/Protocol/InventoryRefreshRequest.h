#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E058: 0x00932E10 opens Inventory and sends C803 with no payload.
    // 0x009A6D30 also refreshes an already open inventory after migration.
    struct InventoryRefreshRequest
    {
        static constexpr std::uint16_t MessageId = 0x0323;
        TcpPacket Encode() const;
        static InventoryRefreshRequest Decode(const TcpPacket& packet);
    };
}
