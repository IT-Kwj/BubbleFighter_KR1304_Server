#include "GameBubbleExitNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleExitNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(SourceObjectId);
        return {MessageId, writer.TakeBytes()};
    }

    GameBubbleExitNotification GameBubbleExitNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble message id."); }
        PacketReader reader(packet.Payload);
        GameBubbleExitNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        result.SourceObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
