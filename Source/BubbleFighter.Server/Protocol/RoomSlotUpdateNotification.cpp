#include "RoomSlotUpdateNotification.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket RoomSlotUpdateNotification::Encode() const
    {
        PacketWriter writer;
        std::size_t budget = GotoRoomNotification::MaxTotalCollectionEntries;
        writer.WriteUInt32(Slot);
        WriteSessionMember(writer, Member, budget);
        WriteSessionRecords(writer, RecordsAt564, budget);
        WriteSessionRecords(writer, RecordsAt588, budget);
        WriteSessionTeam(writer, Team, budget);
        return {MessageId, writer.TakeBytes()};
    }

    RoomSlotUpdateNotification RoomSlotUpdateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected slot-update notification id."); }
        PacketReader reader(packet.Payload);
        std::size_t budget = GotoRoomNotification::MaxTotalCollectionEntries;
        RoomSlotUpdateNotification message;
        message.Slot = reader.ReadUInt32();
        message.Member = ReadSessionMember(reader, budget);
        message.RecordsAt564 = ReadSessionRecords(reader, budget);
        message.RecordsAt588 = ReadSessionRecords(reader, budget);
        message.Team = ReadSessionTeam(reader, budget);
        reader.RequireEnd();
        return message;
    }
}
