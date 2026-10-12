#include "GameRespawnCompletionNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameRespawnCompletionNotification::Encode() const
    {
        PacketWriter writer;
        Parameters.WriteTo(writer);
        writer.WriteUInt8(ByteAt20);
        writer.WriteUInt32(ValueAt21);
        return {MessageId, writer.TakeBytes()};
    }
    GameRespawnCompletionNotification GameRespawnCompletionNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid respawn completion notification."); }
        PacketReader reader(packet.Payload);
        GameRespawnCompletionNotification result;
        result.Parameters = RespawnParameters::ReadFrom(reader);
        result.ByteAt20 = reader.ReadUInt8();
        result.ValueAt21 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
