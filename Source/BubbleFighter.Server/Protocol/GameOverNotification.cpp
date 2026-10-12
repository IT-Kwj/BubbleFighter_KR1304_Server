#include "GameOverNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameOverNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Reason);
        writer.WriteUInt32(ValueAt4);
        return {MessageId, writer.TakeBytes()};
    }
    GameOverNotification GameOverNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game-over message."); }
        PacketReader reader(packet.Payload);
        GameOverNotification result{reader.ReadUInt32(), reader.ReadUInt32()};
        reader.RequireEnd();
        return result;
    }
}
