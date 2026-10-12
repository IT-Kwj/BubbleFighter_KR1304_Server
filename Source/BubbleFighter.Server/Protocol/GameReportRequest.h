#pragma once
#include "TcpPacketCodec.h"
#include <array>

namespace BubbleFighter::Server::Protocol
{
    // E071: 0x009E80F0 -> 0x0113F230, 30 uint32 fields (120 payload bytes).
    // Reported counters are untrusted and their individual meanings unresolved.
    struct GameReportRequest
    {
        static constexpr std::uint16_t MessageId = 103;
        std::uint32_t RandomSeed = 0;
        std::array<std::uint32_t, 29> ValuesAt4{};
        TcpPacket Encode() const;
        static GameReportRequest Decode(const TcpPacket& packet);
        bool operator==(const GameReportRequest&) const = default;
    };
}
