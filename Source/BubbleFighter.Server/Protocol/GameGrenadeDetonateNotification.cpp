#include "GameGrenadeDetonateNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameGrenadeDetonateNotification::Encode() const
    {
        if (Values.size() > MaxValues) { throw std::length_error("Grenade extension exceeds payload policy."); }
        PacketWriter writer;
        writer.WriteUInt32(SourceDamagerKey);
        for (const auto value : PositionBits) { writer.WriteUInt32(value); }
        for (const auto value : VectorBits) { writer.WriteUInt32(value); }
        writer.WriteUInt32(DamagerKey);
        writer.WriteUInt32(PackedTime);
        writer.WriteUInt32(static_cast<std::uint32_t>(Values.size()));
        for (const auto value : Values) { writer.WriteUInt32(value); }
        return {MessageId, writer.TakeBytes()};
    }

    GameGrenadeDetonateNotification GameGrenadeDetonateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid grenade detonation message."); }
        if (packet.Payload.size() > TcpPacketCodec::DefaultMaxPayloadSize)
        { throw std::length_error("Grenade extension exceeds payload policy."); }
        PacketReader reader(packet.Payload);
        GameGrenadeDetonateNotification value;
        value.SourceDamagerKey = reader.ReadUInt32();
        for (auto& coordinate : value.PositionBits) { coordinate = reader.ReadUInt32(); }
        for (auto& coordinate : value.VectorBits) { coordinate = reader.ReadUInt32(); }
        value.DamagerKey = reader.ReadUInt32();
        value.PackedTime = reader.ReadUInt32();
        const auto count = reader.ReadUInt32();
        if (count > MaxValues || reader.RemainingBytes() % 4 != 0 || count != reader.RemainingBytes() / 4)
        { throw std::runtime_error("Invalid grenade extension count."); }
        value.Values.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) { value.Values.push_back(reader.ReadUInt32()); }
        reader.RequireEnd();
        return value;
    }
}
