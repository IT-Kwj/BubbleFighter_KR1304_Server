#include "GameActorDeathNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameActorDeathNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }

    GameActorDeathNotification GameActorDeathNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor death message."); }
        PacketReader reader(packet.Payload);
        GameActorDeathNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
