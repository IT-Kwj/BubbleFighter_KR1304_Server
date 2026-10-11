#pragma once

#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E024: gspRepMoveInto, read by BFCLIENT02 at 0x009A6D30.
    struct MoveIntoResponse
    {
        static constexpr std::uint16_t MessageId = 0x000B;

        // The client converts every nonzero value to true. Keep all wire bits.
        std::uint32_t AcceptedValue = 0;
        std::uint32_t ChannelCategory = 0;
        std::uint32_t UnknownConnectionValue = 0;

        bool IsAccepted() const noexcept;
        TcpPacket Encode() const;
        static MoveIntoResponse Decode(const TcpPacket& packet);
        bool operator==(const MoveIntoResponse&) const = default;
    };
}
