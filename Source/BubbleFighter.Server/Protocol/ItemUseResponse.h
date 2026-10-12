#pragma once
#include "TcpPacketCodec.h"
#include "ItemUseArguments.h"

namespace BubbleFighter::Server::Protocol
{
    struct ItemUseResponse
    {
        static constexpr std::uint16_t MessageId = 813;
        // Result meanings are function-specific. These are Function=3 only.
        static constexpr std::uint32_t RenameSuccess = 0, RenameNameTaken = 10,
            RenameBannedName = 11, RenameInvalidLength = 12, RenameInvalidCharacters = 13;
        std::uint32_t Result = 0;
        std::uint32_t ItemId = 0;
        std::vector<std::u16string> Arguments;
        TcpPacket Encode() const;
        static ItemUseResponse Decode(const TcpPacket& packet);
        bool operator==(const ItemUseResponse&) const = default;
    };
}
