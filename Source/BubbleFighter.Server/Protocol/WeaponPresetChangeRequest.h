#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    struct WeaponPresetChangeRequest
    {
        static constexpr std::uint16_t MessageId = 832;
        static constexpr std::uint8_t MaxPresetIndex = 4;
        static constexpr std::uint32_t MenuObjectId = 0xFFFFFFFF;

        // The menu writer supplies MenuObjectId. Battle supplies a local object
        // ID; decoding that ID does not establish ownership or authorize play.
        std::uint32_t ObjectId = MenuObjectId;
        std::uint8_t PresetIndex = 0;

        TcpPacket Encode() const;
        static WeaponPresetChangeRequest Decode(const TcpPacket& packet);
        bool operator==(const WeaponPresetChangeRequest&) const = default;
    };
}
