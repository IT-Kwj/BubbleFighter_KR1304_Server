#include "GameBubbleActionCompleteNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleActionCompleteNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    GameBubbleActionCompleteNotification GameBubbleActionCompleteNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble action completion id."); }
        PacketReader reader(packet.Payload);
        GameBubbleActionCompleteNotification value{reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}
