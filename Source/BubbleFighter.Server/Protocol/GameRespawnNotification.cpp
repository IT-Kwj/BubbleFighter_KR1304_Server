#include "GameRespawnNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameRespawnNotification::Encode() const
    {
        PacketWriter writer;
        Parameters.WriteTo(writer);
        writer.WriteUInt32(ResetItemsValue);
        writer.WriteUInt32(ValueAt24);
        return {MessageId, writer.TakeBytes()};
    }
    GameRespawnNotification GameRespawnNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game respawn notification."); }
        PacketReader reader(packet.Payload);
        GameRespawnNotification result;
        result.Parameters = RespawnParameters::ReadFrom(reader);
        result.ResetItemsValue = reader.ReadUInt32();
        result.ValueAt24 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
