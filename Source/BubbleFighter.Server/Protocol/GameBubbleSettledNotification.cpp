#include "GameBubbleSettledNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleSettledNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    GameBubbleSettledNotification GameBubbleSettledNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble settled message."); }
        PacketReader reader(packet.Payload);
        GameBubbleSettledNotification result{reader.ReadUInt32()};
        reader.RequireEnd();
        return result;
    }
}
