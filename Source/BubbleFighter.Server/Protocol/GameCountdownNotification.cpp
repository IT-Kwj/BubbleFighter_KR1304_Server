#include "GameCountdownNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameCountdownNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(DurationMilliseconds);
        return {MessageId, writer.TakeBytes()};
    }

    GameCountdownNotification GameCountdownNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game countdown message."); }
        PacketReader reader(packet.Payload);
        GameCountdownNotification result;
        result.DurationMilliseconds = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
