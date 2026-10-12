#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E085: 0x00708660. Field offsets are wire offsets, not native padding.
    // The 0x006DDBA0 switch is not a global EventCode whitelist.
    struct ClientToServerMessage0x07ED
    {
        static constexpr std::uint16_t MessageId = 2029;
        std::uint8_t EventCode = 0;
        std::uint32_t ValueAt1 = 0;
        std::uint8_t ByteAt5 = 0;
        TcpPacket Encode() const;
        static ClientToServerMessage0x07ED Decode(const TcpPacket& packet);
        bool operator==(const ClientToServerMessage0x07ED&) const = default;
    };
}
