#include "GameBubbleActionEndNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleActionEndNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    GameBubbleActionEndNotification GameBubbleActionEndNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble action end id."); }
        PacketReader reader(packet.Payload);
        GameBubbleActionEndNotification value{reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}
