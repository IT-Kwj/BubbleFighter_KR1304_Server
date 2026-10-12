#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E085: 0x006DDBA0. Offsets below refer to the packed wire payload.
    // Event-specific meanings/authority are not inferred from this layout.
    struct ClientToServerMessage0x0260
    {
        static constexpr std::uint16_t MessageId = 608;
        std::uint8_t EventCode = 0;
        std::uint32_t ValueAt1 = 0;
        std::uint32_t ValueAt5 = 0;
        std::uint8_t ByteAt9 = 0;
        std::uint8_t ByteAt10 = 0;
        std::uint8_t ByteAt11 = 0;
        std::uint16_t ShortAt12 = 0;
        std::uint16_t ShortAt14 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x0260 Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x0260&) const = default;
    };
}
