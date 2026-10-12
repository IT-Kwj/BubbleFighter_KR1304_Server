#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // Legacy type name: 6E42B0 / 6FAAF0 also handles ordinary action-110
    // click-to-pop (8F0EF0), sent by SourceObjectId, contact value 0.
    // First actor is the bubbled target. See BattlePop.md counterevidence.
    struct GameBubbleShotPopRequest
    {
        static constexpr std::uint16_t MessageId = 0x957F;
        std::uint32_t ActorObjectId = 0;
        std::uint32_t SourceObjectId = 0;
        TcpPacket Encode() const;
        static GameBubbleShotPopRequest Decode(const TcpPacket& packet);
        bool operator==(const GameBubbleShotPopRequest&) const = default;
    };
}
