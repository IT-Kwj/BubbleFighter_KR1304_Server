#pragma once

#include "SessionDescriptor.h"
#include "TcpPacketCodec.h"

#include <cstdint>
#include <vector>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R15: S101, LobbyStage 0x00853E90 case 0x65 -> reader
    // 0x0084F550 -> list update 0x00856CD0 -> renderer 0x0063CF90.
    struct RoomListResponse
    {
        static constexpr std::uint16_t MessageId = 0x0065;
        // Local resource limit; descriptors are read by 0x010D5210.
        static constexpr std::uint32_t MaxRooms = 1024;

        // Shown page; the renderer enables "previous" for a nonzero value.
        std::uint32_t Page = 0;
        // Stored as a bool at reader +4; enables the "next" button.
        std::uint32_t HasNextPage = 0;
        // Read into reader +8 and passed to 0x0063CF90, which never uses it.
        std::uint32_t ValueAt8 = 0;
        // Each entry is the same descriptor a room's S117 carries.
        std::vector<SessionDescriptor> Rooms;

        TcpPacket Encode() const;
        static RoomListResponse Decode(const TcpPacket& packet);
        bool operator==(const RoomListResponse&) const = default;
    };
}
