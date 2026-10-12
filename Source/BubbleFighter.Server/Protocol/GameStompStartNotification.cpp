#include "GameStompStartNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameStompStartNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(TargetObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    GameStompStartNotification GameStompStartNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble stomp start."); }
        PacketReader reader(packet.Payload);
        GameStompStartNotification value;
        value.ActorObjectId = reader.ReadUInt32();
        value.TargetObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return value;
    }
}
