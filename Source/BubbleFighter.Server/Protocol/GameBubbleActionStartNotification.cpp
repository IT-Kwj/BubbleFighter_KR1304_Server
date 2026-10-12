#include "GameBubbleActionStartNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleActionStartNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(TargetObjectId);
        writer.WriteUInt32(Variant);
        return {MessageId, writer.TakeBytes()};
    }
    GameBubbleActionStartNotification GameBubbleActionStartNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble action start id."); }
        PacketReader reader(packet.Payload);
        GameBubbleActionStartNotification value;
        value.ActorObjectId = reader.ReadUInt32();
        value.TargetObjectId = reader.ReadUInt32();
        value.Variant = reader.ReadUInt32();
        reader.RequireEnd();
        return value;
    }
}
