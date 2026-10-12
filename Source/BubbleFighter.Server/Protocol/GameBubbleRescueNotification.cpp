#include "GameBubbleRescueNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleRescueNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(HelperObjectId);
        writer.WriteUInt32(ValueAt8);
        writer.WriteUInt8(Kind);
        return {MessageId, writer.TakeBytes()};
    }

    GameBubbleRescueNotification GameBubbleRescueNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble message id."); }
        PacketReader reader(packet.Payload);
        GameBubbleRescueNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        result.HelperObjectId = reader.ReadUInt32();
        result.ValueAt8 = reader.ReadUInt32();
        result.Kind = reader.ReadUInt8();
        reader.RequireEnd();
        return result;
    }
}
