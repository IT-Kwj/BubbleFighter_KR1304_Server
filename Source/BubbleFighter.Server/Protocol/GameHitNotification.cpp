#include "GameHitNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameHitNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(DamagerKey);
        writer.WriteUInt32(TargetObjectId);
        for (const auto value : Position) { writer.WriteUInt16(value); }
        writer.WriteUInt8(HitKind);
        writer.WriteUInt32(KeepDamagerValue);
        return {MessageId, writer.TakeBytes()};
    }

    GameHitNotification GameHitNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid hit notification."); }
        PacketReader reader(packet.Payload);
        GameHitNotification value;
        value.DamagerKey = reader.ReadUInt32();
        value.TargetObjectId = reader.ReadUInt32();
        for (auto& coordinate : value.Position) { coordinate = reader.ReadUInt16(); }
        value.HitKind = reader.ReadUInt8();
        value.KeepDamagerValue = reader.ReadUInt32();
        reader.RequireEnd();
        return value;
    }
}
