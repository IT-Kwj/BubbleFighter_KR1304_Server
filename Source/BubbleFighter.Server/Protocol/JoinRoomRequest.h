#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>
#include <string>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R17: C403, written by the room-list join handler at
    // 0x00855454 (function 0x00854C90).
    struct JoinRoomRequest
    {
        static constexpr std::uint16_t MessageId = 0x0193;

        // The listed descriptor's ShortAt0, sign-extended to 32 bits.
        std::uint32_t RoomId = 0;
        // Empty unless the listed room has BooleanAt38 set; the client then
        // asks for it in a dialog before sending.
        std::u16string Password;

        TcpPacket Encode() const;
        static JoinRoomRequest Decode(const TcpPacket& packet);
        bool operator==(const JoinRoomRequest&) const = default;
    };
}
