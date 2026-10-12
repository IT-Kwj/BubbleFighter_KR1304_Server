#pragma once

#include "TcpPacketCodec.h"

#include <cstdint>

namespace BubbleFighter::Server::Protocol
{
    // RoomCreation R15: C401, written only by 0x00858E70 (callers: room list
    // widget 0x0063E5D0 and LobbyStage 0x008530C0/0x00853930).
    struct RoomListRequest
    {
        static constexpr std::uint16_t MessageId = 0x0191;
        // 0x00858E70 writes this constant after the page index.
        static constexpr std::uint16_t ClientPageSize = 10;

        // Zero-based: 0x0063CF90 enables the previous-page button for page > 0.
        std::uint16_t Page = 0;
        std::uint16_t PageSize = ClientPageSize;

        TcpPacket Encode() const;
        static RoomListRequest Decode(const TcpPacket& packet);
        bool operator==(const RoomListRequest&) const = default;
    };
}
