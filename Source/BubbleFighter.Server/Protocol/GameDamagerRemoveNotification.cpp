#include "GameDamagerRemoveNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameDamagerRemoveNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(DamagerKey);
        return {MessageId, writer.TakeBytes()};
    }

    GameDamagerRemoveNotification GameDamagerRemoveNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid damager removal notification."); }
        PacketReader reader(packet.Payload);
        GameDamagerRemoveNotification value{reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}
