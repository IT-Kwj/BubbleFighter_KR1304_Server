#include "GameActorRespawnWaitNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameActorRespawnWaitNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }

    GameActorRespawnWaitNotification GameActorRespawnWaitNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor respawn wait message."); }
        PacketReader reader(packet.Payload);
        GameActorRespawnWaitNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
