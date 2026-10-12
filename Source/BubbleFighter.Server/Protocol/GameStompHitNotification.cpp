#include "GameStompHitNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameStompHitNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(TargetObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    GameStompHitNotification GameStompHitNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble stomp hit."); }
        PacketReader reader(packet.Payload);
        GameStompHitNotification value;
        value.ActorObjectId = reader.ReadUInt32();
        value.TargetObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return value;
    }
}
