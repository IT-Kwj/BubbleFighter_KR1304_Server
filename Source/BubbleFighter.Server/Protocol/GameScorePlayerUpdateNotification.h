#pragma once
#include "GameScoreValueUpdate.h"
#include "TcpPacketCodec.h"
#include <map>

namespace BubbleFighter::Server::Protocol
{
    // E074: S655 map of native slot -> ordered value updates. The native map
    // reader rejects duplicate slots. Repeated field indices remain ordered.
    struct GameScorePlayerUpdateNotification
    {
        static constexpr std::uint16_t MessageId = 655;
        static constexpr std::size_t MaxPlayers = 36;
        static constexpr std::size_t MaxPayloadBytes = 4 + 8 * MaxPlayers + 3 * GameScoreValueUpdate::MaxTotalEntries;
        std::map<std::uint32_t, std::vector<GameScoreValueUpdate>> Players;
        TcpPacket Encode() const;
        static GameScorePlayerUpdateNotification Decode(const TcpPacket& packet);
        bool operator==(const GameScorePlayerUpdateNotification&) const = default;
    };
}
