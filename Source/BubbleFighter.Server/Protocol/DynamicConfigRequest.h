#pragma once

#include "TcpPacketCodec.h"
#include <array>
#include <optional>

namespace BubbleFighter::Server::Protocol
{
    struct DynamicConfigRequest
    {
        static constexpr std::uint16_t MessageId = 0x05DC;
        std::array<std::optional<std::uint32_t>, 32> Checksums;
        TcpPacket Encode() const;
        static DynamicConfigRequest Decode(const TcpPacket& packet);
        bool operator==(const DynamicConfigRequest&) const = default;
    };
}
