#include "GameScorePlayerUpdateNotification.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameScorePlayerUpdateNotification::Encode() const
    {
        if (Players.size() > MaxPlayers) { throw std::length_error("Score update map exceeds native slot count."); }
        std::size_t budget = GameScoreValueUpdate::MaxTotalEntries;
        for (const auto& [slot, updates] : Players)
        {
            if (slot >= MaxPlayers) { throw std::invalid_argument("Invalid score update player slot."); }
            if (updates.size() > budget) { throw std::length_error("Score update exceeds total entry budget."); }
            budget -= updates.size();
        }
        PacketWriter writer;
        writer.WriteUInt32(static_cast<std::uint32_t>(Players.size()));
        for (const auto& [slot, updates] : Players)
        { writer.WriteUInt32(slot); GameScoreValueUpdate::WriteList(writer, updates); }
        return {MessageId, writer.TakeBytes()};
    }

    GameScorePlayerUpdateNotification GameScorePlayerUpdateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid player score update message."); }
        if (packet.Payload.size() > MaxPayloadBytes) { throw std::length_error("Player score update payload exceeds local limit."); }
        PacketReader reader(packet.Payload);
        const auto count = reader.ReadUInt32();
        if (count > MaxPlayers || count > reader.RemainingBytes() / 8)
        { throw std::length_error("Score update player count invalid or truncated."); }
        GameScorePlayerUpdateNotification result;
        std::size_t budget = GameScoreValueUpdate::MaxTotalEntries;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto slot = reader.ReadUInt32();
            if (slot >= MaxPlayers || result.Players.contains(slot))
            { throw std::invalid_argument("Invalid or repeated score update player slot."); }
            result.Players.emplace(slot, GameScoreValueUpdate::ReadList(reader, budget));
        }
        reader.RequireEnd();
        return result;
    }
}
