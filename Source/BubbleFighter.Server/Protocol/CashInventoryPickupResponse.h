#pragma once

#include "TcpPacketCodec.h"
#include <string>

namespace BubbleFighter::Server::Protocol
{
    struct CashInventoryPickupResponse
    {
        static constexpr std::uint16_t MessageId = 835;
        static constexpr std::uint32_t Success = 1;
        static constexpr std::uint32_t Failed = 2;
        static constexpr std::uint32_t AlreadyOwned = 8;
        std::uint32_t SlotId = 0;
        std::uint32_t Result = 0;
        std::uint32_t ValueAt8 = 0; // logged only when positive with nonempty text
        std::u16string TextAt12;
        TcpPacket Encode() const;
        static CashInventoryPickupResponse Decode(const TcpPacket& packet);
        bool operator==(const CashInventoryPickupResponse&) const = default;
    };
}
