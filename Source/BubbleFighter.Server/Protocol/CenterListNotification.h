#pragma once

#include "TcpPacketCodec.h"

#include <array>
#include <cstdint>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    struct CenterGateInfo
    {
        std::uint32_t UnknownValueAt4 = 0;
        std::uint32_t UnknownValueAt8 = 0;
        std::uint32_t UnknownValueAt12 = 0;
        std::array<std::uint8_t, 4> Ipv4Address{};
        std::uint16_t Port = 0;
        bool operator==(const CenterGateInfo&) const = default;
    };

    struct CenterListNotification
    {
        static constexpr std::uint16_t MessageId = 0x0011;
        // Local allocation limit, not a claim about the original server.
        static constexpr std::uint32_t MaxEntries = 1024;
        std::vector<CenterGateInfo> Gates;
        TcpPacket Encode() const;
        static CenterListNotification Decode(const TcpPacket& packet);
        bool operator==(const CenterListNotification&) const = default;
    };
}
