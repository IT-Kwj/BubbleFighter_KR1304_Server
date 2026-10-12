#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R16: S502, room dispatcher 0x009D9930 case 0x1F6 ->
    // 0x009DA3C0. Stores the slot as the room host (context +552, the S117
    // ValueAt544 holder), clears that slot's ready byte and refreshes the
    // room stage buttons.
    struct RoomHostChangedNotification
    {
        static constexpr std::uint16_t MessageId = 0x01F6;

        std::uint32_t HostSlot = 0;

        TcpPacket Encode() const;
        static RoomHostChangedNotification Decode(const TcpPacket& packet);
        bool operator==(const RoomHostChangedNotification&) const = default;
    };
}
