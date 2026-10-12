#pragma once
#include "TcpPacketCodec.h"
#include "ItemUseArguments.h"

namespace BubbleFighter::Server::Protocol
{
    struct ItemUseRequest
    {
        static constexpr std::uint16_t MessageId = 806;
        std::uint32_t ItemId = 0;
        // Ordinary menu callers pass 1; meaning for other functions is unknown.
        std::uint32_t ValueAt4 = 0;
        std::vector<std::u16string> Arguments;
        TcpPacket Encode() const;
        static ItemUseRequest Decode(const TcpPacket& packet);
        bool operator==(const ItemUseRequest&) const = default;
    };
}
