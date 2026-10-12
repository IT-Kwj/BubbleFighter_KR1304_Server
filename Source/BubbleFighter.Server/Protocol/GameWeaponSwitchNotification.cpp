#include "GameWeaponSwitchNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameWeaponSwitchNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(WeaponId);
        writer.WriteUInt32(WeaponIndex);
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(TimeValue);
        writer.WriteUInt32(SelectValue);
        writer.WriteUInt32(ValueAt20);
        return {MessageId, writer.TakeBytes()};
    }
    GameWeaponSwitchNotification GameWeaponSwitchNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid weapon switch message."); }
        PacketReader reader(packet.Payload);
        GameWeaponSwitchNotification result;
        result.WeaponId = reader.ReadUInt32();
        result.WeaponIndex = reader.ReadUInt32();
        result.ActorObjectId = reader.ReadUInt32();
        result.TimeValue = reader.ReadUInt32();
        result.SelectValue = reader.ReadUInt32();
        result.ValueAt20 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
