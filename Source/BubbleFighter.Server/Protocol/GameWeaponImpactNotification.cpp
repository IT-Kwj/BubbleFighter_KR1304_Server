#include "GameWeaponImpactNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameWeaponImpactNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(DamagerKey);
        for (const auto value : PositionBits) { writer.WriteUInt32(value); }
        writer.WriteUInt32(SourceObjectId);
        writer.WriteUInt32(TargetObjectId);
        return {MessageId, writer.TakeBytes()};
    }

    GameWeaponImpactNotification GameWeaponImpactNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid weapon impact message."); }
        PacketReader reader(packet.Payload);
        GameWeaponImpactNotification value;
        value.DamagerKey = reader.ReadUInt32();
        for (auto& coordinate : value.PositionBits) { coordinate = reader.ReadUInt32(); }
        value.SourceObjectId = reader.ReadUInt32();
        value.TargetObjectId = reader.ReadUInt32();
        reader.RequireEnd();
        return value;
    }
}
