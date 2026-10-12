#include "GameActorDamageResultNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameActorDamageResultNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(SourceObjectId);
        for (auto bits : VectorBits) { writer.WriteUInt32(bits); }
        writer.WriteUInt32(WeaponId);
        writer.WriteUInt32(DirectDeathValue);
        writer.WriteUInt32(FullHealthHitValue);
        writer.WriteUInt32(TimeValue);
        writer.WriteUInt32(ValueAt36);
        return {MessageId, writer.TakeBytes()};
    }

    GameActorDamageResultNotification GameActorDamageResultNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor damage result notification."); }
        PacketReader reader(packet.Payload);
        GameActorDamageResultNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        result.SourceObjectId = reader.ReadUInt32();
        for (auto& bits : result.VectorBits) { bits = reader.ReadUInt32(); }
        result.WeaponId = reader.ReadUInt32();
        result.DirectDeathValue = reader.ReadUInt32();
        result.FullHealthHitValue = reader.ReadUInt32();
        result.TimeValue = reader.ReadUInt32();
        result.ValueAt36 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
