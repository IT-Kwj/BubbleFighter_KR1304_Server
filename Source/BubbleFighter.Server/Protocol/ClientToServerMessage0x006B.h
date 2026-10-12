#pragma once

#include "TcpPacketCodec.h"
#include <array>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // E068: start request emitted by 0x0099F580. Selector zero now chooses
    // a training map, not the older client's prologue. No mode is inferred.
    struct ClientToServerMessage0x006B
    {
        static constexpr std::uint16_t MessageId = 107;
        static constexpr std::size_t MaxPayloadBytes = 65536;
        // Client object offsets 0..48. Resource names/units are in TutorialEntry.md.
        std::array<std::uint32_t, 13> Rules{};
        std::uint8_t RuleFlags = 0; // Client offset 52; exactly ONE byte on wire.
        std::u16string Directory;
        std::u16string MapName;
        std::u16string MapKey;
        std::uint32_t Difficulty = 0;

        TcpPacket Encode() const;
        static ClientToServerMessage0x006B Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x006B&) const = default;
    };
}
