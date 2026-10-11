#pragma once

#include "TcpPacketCodec.h"
#include <array>
#include <optional>

namespace BubbleFighter::Server::Protocol
{
    struct DynamicConfigResponse
    {
        static constexpr std::uint16_t MessageId = 0x05DC;
        // Local aggregate bound, below the TCP payload limit including framing.
        static constexpr std::size_t MaxCompressedBytes = 512 * 1024;
        std::array<std::optional<std::vector<std::uint8_t>>, 32> Records;
        TcpPacket Encode() const;
        static DynamicConfigResponse Decode(const TcpPacket& packet);
        bool operator==(const DynamicConfigResponse&) const = default;
    };
}
