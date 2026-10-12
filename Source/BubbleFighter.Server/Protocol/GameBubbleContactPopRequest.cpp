#include "GameBubbleContactPopRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleContactPopRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(SourceObjectId);
        return {MessageId, writer.TakeBytes()};
    }

    GameBubbleContactPopRequest GameBubbleContactPopRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble message id."); }
        PacketReader reader(packet.Payload);
        GameBubbleContactPopRequest result;
        result.ActorObjectId = reader.ReadUInt32();
        result.SourceObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
