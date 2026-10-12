#pragma once
#include "RespawnParameters.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // Main TCP S632, consumed by 0x006F5E50. ValueAt24 is written to
    // the scene member's +304 field; its business meaning is not confirmed.
    struct GameRespawnNotification
    {
        static constexpr std::uint16_t MessageId = 632;
        RespawnParameters Parameters;
        std::uint32_t ResetItemsValue = 0;
        std::uint32_t ValueAt24 = 0;
        TcpPacket Encode() const;
        static GameRespawnNotification Decode(const TcpPacket& packet);
        bool operator==(const GameRespawnNotification&) const = default;
    };
}
