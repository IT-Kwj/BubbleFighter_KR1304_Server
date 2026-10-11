#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct UnequipItemRequest
    {
        static constexpr std::uint16_t MessageId = 805;
        // Current client writes the full 64-bit identifier. Ordinary item IDs
        // and ArticleSlot instances are separated by the business layer.
        std::uint64_t ItemIdentifier = 0;
        TcpPacket Encode() const;
        static UnequipItemRequest Decode(const TcpPacket& packet);
    };
}
