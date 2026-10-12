#pragma once
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // BFCLIENT02 0x009E33B4 consumes no payload; 0x009EA690 clears the game
    // and selects RoomStage=4 for the supported ordinary mode/flags branch.
    struct GameReturnToRoomNotification
    {
        static constexpr std::uint16_t MessageId = 620;
        TcpPacket Encode() const;
        static GameReturnToRoomNotification Decode(const TcpPacket& packet);
        bool operator==(const GameReturnToRoomNotification&) const = default;
    };
}
