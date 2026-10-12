#include "GameScoreUpdateNotification.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    void GameScoreUpdateNotification::Validate() const
    {
        if (GroupIndex > 36 || (FirstSlot >= 36 && FirstSlot < 128) || (SecondSlot >= 36 && SecondSlot < 128))
        { throw std::invalid_argument("Score update group or player exceeds native capacity."); }
        std::size_t budget = GameScoreValueUpdate::MaxTotalEntries;
        for (const auto* updates : {&FirstUpdates, &SecondUpdates, &GroupUpdates})
        {
            if (updates->size() > budget) { throw std::length_error("Score update exceeds total entry budget."); }
            budget -= updates->size();
        }
        for (const auto& update : GroupUpdates)
        {
            // This list sign-extends its byte key and indexes a native 37-entry array.
            if (update.Index > 36) { throw std::invalid_argument("Invalid score update group index."); }
        }
    }

    TcpPacket GameScoreUpdateNotification::Encode() const
    {
        Validate();
        PacketWriter writer;
        writer.WriteUInt8(GroupIndex); writer.WriteUInt16(GroupValue);
        writer.WriteUInt8(FirstSlot); writer.WriteUInt8(SecondSlot);
        GameScoreValueUpdate::WriteList(writer, FirstUpdates);
        GameScoreValueUpdate::WriteList(writer, SecondUpdates);
        GameScoreValueUpdate::WriteList(writer, GroupUpdates);
        return {MessageId, writer.TakeBytes()};
    }

    GameScoreUpdateNotification GameScoreUpdateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid score update message."); }
        if (packet.Payload.size() > MaxPayloadBytes) { throw std::length_error("Score update payload exceeds local limit."); }
        PacketReader reader(packet.Payload);
        GameScoreUpdateNotification result;
        result.GroupIndex = reader.ReadUInt8(); result.GroupValue = reader.ReadUInt16();
        result.FirstSlot = reader.ReadUInt8(); result.SecondSlot = reader.ReadUInt8();
        std::size_t budget = GameScoreValueUpdate::MaxTotalEntries;
        result.FirstUpdates = GameScoreValueUpdate::ReadList(reader, budget);
        result.SecondUpdates = GameScoreValueUpdate::ReadList(reader, budget);
        result.GroupUpdates = GameScoreValueUpdate::ReadList(reader, budget);
        reader.RequireEnd(); result.Validate();
        return result;
    }
}
