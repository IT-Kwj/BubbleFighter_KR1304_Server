#include "GameMeleeAttackNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameMeleeAttackNotification::Encode() const
    {
        if (Values.size() > MaxValues) { throw std::length_error("Melee extension exceeds payload policy."); }
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt16(WeaponId);
        writer.WriteUInt8(ByteAt6);
        writer.WriteUInt32(ShotSequence);
        writer.WriteUInt32(PackedTime);
        writer.WriteUInt32(static_cast<std::uint32_t>(Values.size()));
        for (const auto value : Values) { writer.WriteUInt32(value); }
        return {MessageId, writer.TakeBytes()};
    }

    GameMeleeAttackNotification GameMeleeAttackNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid melee attack message."); }
        if (packet.Payload.size() > TcpPacketCodec::DefaultMaxPayloadSize)
        { throw std::length_error("Melee extension exceeds payload policy."); }
        PacketReader reader(packet.Payload);
        GameMeleeAttackNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        result.WeaponId = reader.ReadUInt16();
        result.ByteAt6 = reader.ReadUInt8();
        result.ShotSequence = reader.ReadUInt32();
        result.PackedTime = reader.ReadUInt32();
        const auto count = reader.ReadUInt32();
        if (count > MaxValues || reader.RemainingBytes() % 4 != 0 || count != reader.RemainingBytes() / 4)
        { throw std::runtime_error("Invalid melee extension count."); }
        result.Values.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) { result.Values.push_back(reader.ReadUInt32()); }
        reader.RequireEnd();
        return result;
    }
}
