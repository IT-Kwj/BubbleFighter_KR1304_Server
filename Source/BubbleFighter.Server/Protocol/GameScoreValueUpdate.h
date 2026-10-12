#pragma once
#include "PacketReader.h"
#include "PacketWriter.h"
#include <span>

namespace BubbleFighter::Server::Protocol
{
    // E074: each list element is byte index + uint16 value, with no padding.
    // A player's index selects a field; a group list's index selects a group.
    struct GameScoreValueUpdate
    {
        static constexpr std::size_t MaxTotalEntries = 1024; // Local packet-wide budget.
        std::uint8_t Index = 0;
        std::uint16_t Value = 0;
        static void WriteList(PacketWriter& writer, std::span<const GameScoreValueUpdate> updates);
        static std::vector<GameScoreValueUpdate> ReadList(PacketReader& reader, std::size_t& budget);
        bool operator==(const GameScoreValueUpdate&) const = default;
    };
}
