#pragma once
#include "RespawnParameters.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // Main TCP C602. itemResetOnRespawn[Single] supplies the trailing
    // uint32 at the sender; no server authorization is implied by decoding.
    struct GameRespawnRequest
    {
        static constexpr std::uint16_t MessageId = 602;
        RespawnParameters Parameters;
        std::uint32_t ResetItemsValue = 0;
        TcpPacket Encode() const;
        static GameRespawnRequest Decode(const TcpPacket& packet);
        bool operator==(const GameRespawnRequest&) const = default;
    };
}
