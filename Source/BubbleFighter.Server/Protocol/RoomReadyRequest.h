#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R19: C505, written by the room stage update 0x009785E0 when
    // the local room state changes. State 0 -> {0,0,0}, 1 (ready button
    // +0x644) -> {1,0,0}, 3 -> {0,1,0}, 2 -> {0,0,1}; states 2 and 3 come
    // from client UI flags (0x018E566C +260 / +256) whose meaning is unknown.
    // Values are zero-extended bytes.
    struct RoomReadyRequest
    {
        static constexpr std::uint16_t MessageId = 0x01F9;

        std::uint32_t Ready = 0;
        std::uint32_t ValueAt4 = 0;
        std::uint32_t ValueAt8 = 0;

        TcpPacket Encode() const;
        static RoomReadyRequest Decode(const TcpPacket& packet);
        bool operator==(const RoomReadyRequest&) const = default;
    };
}
