#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct ShopPurchaseRequest
    {
        static constexpr std::uint16_t MessageId = 809;
        std::uint32_t StockId = 0;
        // 0x00E1D720 widens a native byte to uint32. Ordinary shop callers
        // pass zero; other values are not assigned gift/purchase semantics.
        std::uint32_t ValueAt4 = 0;
        TcpPacket Encode() const;
        static ShopPurchaseRequest Decode(const TcpPacket& packet);
        bool operator==(const ShopPurchaseRequest&) const = default;
    };
}
