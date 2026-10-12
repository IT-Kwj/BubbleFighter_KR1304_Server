#include "GameResultNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameResultNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        writer.WriteUInt32(ElapsedMilliseconds);
        writer.WriteUInt8(ByteAt8);
        writer.WriteUInt8(ByteAt9);
        return {MessageId, writer.TakeBytes()};
    }
    GameResultNotification GameResultNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game result message."); }
        PacketReader reader(packet.Payload);
        GameResultNotification result{reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt8(), reader.ReadUInt8()};
        reader.RequireEnd();
        return result;
    }
}
