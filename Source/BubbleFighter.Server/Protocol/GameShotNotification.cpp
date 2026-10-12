#include "GameShotNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameShotNotification::Encode() const
    {
        PacketWriter writer;
        Shot.Encode(writer);
        return {MessageId, writer.TakeBytes()};
    }

    GameShotNotification GameShotNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid shot notification."); }
        PacketReader reader(packet.Payload);
        GameShotNotification result{CompressedShotRecord::Decode(reader)};
        reader.RequireEnd();
        return result;
    }
}
