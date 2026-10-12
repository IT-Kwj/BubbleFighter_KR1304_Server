#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct WeaponPresetChangeResponse
    {
        static constexpr std::uint16_t MessageId = 836;
        static constexpr std::uint32_t Success = 0;

        std::uint8_t PresetIndex = 0;
        // Only zero's success meaning is established. Preserve other raw codes
        // for analysis, without inventing individual failure meanings.
        std::uint32_t Result = Success;

        TcpPacket Encode() const;
        static WeaponPresetChangeResponse Decode(const TcpPacket& packet);
        bool operator==(const WeaponPresetChangeResponse&) const = default;
    };
}
