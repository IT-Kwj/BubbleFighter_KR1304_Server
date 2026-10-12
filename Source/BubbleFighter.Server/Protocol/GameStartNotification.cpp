#include "GameStartNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameStartNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(DurationMilliseconds);
        writer.WriteUInt32(RandomSeed);
        return {MessageId, writer.TakeBytes()};
    }

    GameStartNotification GameStartNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game start message."); }
        PacketReader reader(packet.Payload);
        GameStartNotification result;
        result.DurationMilliseconds = reader.ReadUInt32();
        result.RandomSeed = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
