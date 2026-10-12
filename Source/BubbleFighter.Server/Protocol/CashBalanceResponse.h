#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct CashBalanceResponse
    {
        static constexpr std::uint16_t MessageId = 834;
        // E065: both first and second fields are formatted as amounts in the
        // optional cash UI. Only the second is bound to UserStat.Cash. Neither
        // the first nor the third field is established as a result code.
        std::uint32_t ValueAt0 = 0;
        std::uint32_t Cash = 0;
        std::uint32_t ValueAt8 = 0;

        TcpPacket Encode() const;
        static CashBalanceResponse Decode(const TcpPacket& packet);
        bool operator==(const CashBalanceResponse&) const = default;
    };
}
