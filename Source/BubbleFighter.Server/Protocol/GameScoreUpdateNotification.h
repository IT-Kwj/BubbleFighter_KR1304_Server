#pragma once
#include "GameScoreValueUpdate.h"
#include "TcpPacketCodec.h"

namespace BubbleFighter::Server::Protocol
{
    // E074: S985, 0x009E3420 -> 0x009E8B20. Values replace selected fields;
    // they are not increments. No runtime score or reward authority is implied.
    struct GameScoreUpdateNotification
    {
        static constexpr std::uint16_t MessageId = 985;
        static constexpr std::size_t MaxPayloadBytes = 17 + 3 * GameScoreValueUpdate::MaxTotalEntries;
        std::uint8_t GroupIndex = 0; // 0 skips the first group-array write; 1..36 selects an entry.
        std::uint16_t GroupValue = 0;
        std::uint8_t FirstSlot = 0xFF;
        std::uint8_t SecondSlot = 0xFF;
        // Signed-negative slot bytes are ignored by the client. Preserve their
        // raw representation and their lists; do not infer a player identity.
        std::vector<GameScoreValueUpdate> FirstUpdates;
        std::vector<GameScoreValueUpdate> SecondUpdates;
        std::vector<GameScoreValueUpdate> GroupUpdates; // Indices 0..36, used in modes 5/6.
        TcpPacket Encode() const;
        static GameScoreUpdateNotification Decode(const TcpPacket& packet);
        bool operator==(const GameScoreUpdateNotification&) const = default;

    private:
        void Validate() const;
    };
}
