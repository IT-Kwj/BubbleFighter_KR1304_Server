#include "GameRespawnRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameRespawnRequest::Encode() const
    {
        PacketWriter writer;
        Parameters.WriteTo(writer);
        writer.WriteUInt32(ResetItemsValue);
        return {MessageId, writer.TakeBytes()};
    }
    GameRespawnRequest GameRespawnRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game respawn request."); }
        PacketReader reader(packet.Payload);
        GameRespawnRequest result;
        result.Parameters = RespawnParameters::ReadFrom(reader);
        result.ResetItemsValue = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
