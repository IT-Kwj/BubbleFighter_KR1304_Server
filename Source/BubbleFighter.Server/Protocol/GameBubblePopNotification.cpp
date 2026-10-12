#include "GameBubblePopNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubblePopNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ApplyValue);
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(SourceObjectId);
        for (auto value : VectorBits) { writer.WriteUInt32(value); }
        writer.WriteUInt32(ContactValue);
        writer.WriteUInt32(ReasonValue);
        return {MessageId, writer.TakeBytes()};
    }

    GameBubblePopNotification GameBubblePopNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble message id."); }
        PacketReader reader(packet.Payload);
        GameBubblePopNotification result;
        result.ApplyValue = reader.ReadUInt32();
        result.ActorObjectId = reader.ReadUInt32();
        result.SourceObjectId = reader.ReadUInt32();
        for (auto& value : result.VectorBits) { value = reader.ReadUInt32(); }
        result.ContactValue = reader.ReadUInt32();
        result.ReasonValue = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
