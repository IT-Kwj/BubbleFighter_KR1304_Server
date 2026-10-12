#include "RoomHostChangedNotification.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket RoomHostChangedNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(HostSlot);
        return {MessageId, writer.TakeBytes()};
    }

    RoomHostChangedNotification RoomHostChangedNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected host-changed notification id."); }
        PacketReader reader(packet.Payload);
        RoomHostChangedNotification message;
        message.HostSlot = reader.ReadUInt32();
        reader.RequireEnd();
        return message;
    }
}
