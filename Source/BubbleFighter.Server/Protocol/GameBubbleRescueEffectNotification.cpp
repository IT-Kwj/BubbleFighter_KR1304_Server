#include "GameBubbleRescueEffectNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameBubbleRescueEffectNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(HelperObjectId);
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt8(Kind);
        return {MessageId, writer.TakeBytes()};
    }

    GameBubbleRescueEffectNotification GameBubbleRescueEffectNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid bubble message id."); }
        PacketReader reader(packet.Payload);
        GameBubbleRescueEffectNotification result;
        result.HelperObjectId = reader.ReadUInt32();
        result.ActorObjectId = reader.ReadUInt32();
        result.Kind = reader.ReadUInt8();
        reader.RequireEnd();
        return result;
    }
}
