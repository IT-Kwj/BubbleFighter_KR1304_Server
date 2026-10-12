#include "RoomMemberLeftNotification.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket RoomMemberLeftNotification::Encode() const
    {
        PacketWriter writer;
        std::size_t budget = GotoRoomNotification::MaxTotalCollectionEntries;
        writer.WriteUInt32(Slot);
        WriteSessionMember(writer, Member, budget);
        WriteSessionTeam(writer, Team, budget);
        return {MessageId, writer.TakeBytes()};
    }

    RoomMemberLeftNotification RoomMemberLeftNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected member-left notification id."); }
        PacketReader reader(packet.Payload);
        std::size_t budget = GotoRoomNotification::MaxTotalCollectionEntries;
        RoomMemberLeftNotification message;
        message.Slot = reader.ReadUInt32();
        message.Member = ReadSessionMember(reader, budget);
        message.Team = ReadSessionTeam(reader, budget);
        reader.RequireEnd();
        return message;
    }
}
