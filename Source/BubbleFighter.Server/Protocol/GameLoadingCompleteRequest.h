#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E069: GameStage update -> 0x0075B3E0 -> 0x006DC970, after loading.
    // The last two fields are zero in that writer; their semantics are unknown.
    // Codecs preserve them and do not authorize a transition into a game.
    struct GameLoadingCompleteRequest
    {
        static constexpr std::uint16_t MessageId = 578;
        std::uint32_t DelayMilliseconds = 0;
        std::uint32_t ValueAt4 = 0;
        std::uint32_t ValueAt8 = 0;
        std::uint32_t ValueAt12 = 0;

        TcpPacket Encode() const;
        static GameLoadingCompleteRequest Decode(const TcpPacket& packet);
        bool operator==(const GameLoadingCompleteRequest&) const = default;
    };
}
