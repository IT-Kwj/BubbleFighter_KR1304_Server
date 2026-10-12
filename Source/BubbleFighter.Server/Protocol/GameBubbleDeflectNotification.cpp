#include "GameBubbleDeflectNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleDeflectNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(SourceObjectId);
        for (auto value : VectorBits) { writer.WriteUInt32(value); }
        writer.WriteUInt32(ValueAt20);
        writer.WriteUInt32(ContactValue);
        return {MessageId, writer.TakeBytes()};
    }

    GameBubbleDeflectNotification GameBubbleDeflectNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble message id."); }
        PacketReader reader(packet.Payload);
        GameBubbleDeflectNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        result.SourceObjectId = reader.ReadUInt32();
        for (auto& value : result.VectorBits) { value = reader.ReadUInt32(); }
        result.ValueAt20 = reader.ReadUInt32();
        result.ContactValue = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
