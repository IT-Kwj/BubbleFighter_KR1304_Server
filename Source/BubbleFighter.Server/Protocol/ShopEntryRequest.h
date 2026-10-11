#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E061: 0x009328B0 writes the cached catalog revision and category.
    struct ShopEntryRequest
    {
        static constexpr std::uint16_t MessageId = 801;
        std::uint64_t CatalogRevision = 0;
        std::uint32_t SelectedCategory = 0;
        TcpPacket Encode() const;
        static ShopEntryRequest Decode(const TcpPacket& packet);
        bool operator==(const ShopEntryRequest&) const = default;
    };
}
