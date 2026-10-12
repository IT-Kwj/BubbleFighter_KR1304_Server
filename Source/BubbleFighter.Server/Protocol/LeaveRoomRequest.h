#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R18: C404, built empty by 0x0062A880. The room task bar
    // (0x00AA5B50) sends it and changes no stage itself: the server must
    // move the client out of the room.
    struct LeaveRoomRequest
    {
        static constexpr std::uint16_t MessageId = 0x0194;

        TcpPacket Encode() const;
        static LeaveRoomRequest Decode(const TcpPacket& packet);
    };
}
