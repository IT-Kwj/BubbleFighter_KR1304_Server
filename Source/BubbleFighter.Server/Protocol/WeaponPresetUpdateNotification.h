#pragma once

#include "TcpPacketCodec.h"

#include <array>
#include <cstdint>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // E052: current message 837, not legacy 835. Records are three uint32s,
    // not the legacy pair of bytes followed by two uint32s.
    struct WeaponPresetUpdateNotification
    {
        static constexpr std::uint16_t MessageId = 0x0345;
        static constexpr std::uint32_t MaxCollectionEntries = 1024;

        std::uint8_t ByteAt0 = 0;
        std::uint32_t PlayerId = 0;
        std::vector<std::array<std::uint32_t, 3>> Records;
        std::vector<std::uint8_t> Bytes;

        TcpPacket Encode() const;
        static WeaponPresetUpdateNotification Decode(const TcpPacket& packet);
        bool operator==(const WeaponPresetUpdateNotification&) const = default;
    };
}
