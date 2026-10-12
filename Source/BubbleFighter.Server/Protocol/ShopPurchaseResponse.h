#pragma once

#include "TcpPacketCodec.h"

#include <string>

namespace BubbleFighter::Server::Protocol
{
    struct ShopPurchaseResponse
    {
        static constexpr std::uint16_t MessageId = 816;
        // Confirmed by 0x009B6730, not copied from the older server.
        static constexpr std::uint32_t Success = 1;
        static constexpr std::uint32_t InsufficientLucci = 0xFFFFFFF3U; // -13
        static constexpr std::uint32_t InsufficientCash = 36;
        static constexpr std::uint32_t Ineligible = 0xFFFFFF38U; // -200
        static constexpr std::uint32_t AlreadyOwned = 9;

        std::uint32_t StockId = 0;
        // Preserve the exact bits, including negative error codes and unknowns.
        std::uint32_t Result = 0;
        std::u16string TextAt8;
        // A single byte, unlike the widened request byte. Nonzero values
        // select additional shop UI paths; meanings remain unresolved.
        std::uint8_t ByteAt32 = 0;
        std::uint32_t ValueAt36 = 0;
        std::uint32_t ValueAt40 = 0;
        std::uint32_t ValueAt44 = 0;

        TcpPacket Encode() const;
        static ShopPurchaseResponse Decode(const TcpPacket& packet);
        bool operator==(const ShopPurchaseResponse&) const = default;
    };
}
