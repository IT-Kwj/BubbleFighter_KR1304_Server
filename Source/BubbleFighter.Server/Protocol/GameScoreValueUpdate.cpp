#include "GameScoreValueUpdate.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    void GameScoreValueUpdate::WriteList(PacketWriter& writer, std::span<const GameScoreValueUpdate> updates)
    {
        if (updates.size() > MaxTotalEntries) { throw std::length_error("Score update list exceeds local limit."); }
        writer.WriteUInt32(static_cast<std::uint32_t>(updates.size()));
        for (const auto& update : updates) { writer.WriteUInt8(update.Index); writer.WriteUInt16(update.Value); }
    }

    std::vector<GameScoreValueUpdate> GameScoreValueUpdate::ReadList(PacketReader& reader, std::size_t& budget)
    {
        const auto count = reader.ReadUInt32();
        if (count > MaxTotalEntries || count > budget || count > reader.RemainingBytes() / 3)
        { throw std::length_error("Score update count exceeds budget or remaining bytes."); }
        budget -= count;
        std::vector<GameScoreValueUpdate> updates;
        updates.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        { updates.push_back({reader.ReadUInt8(), reader.ReadUInt16()}); }
        return updates;
    }
}
