#include "RoomListRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket RoomListRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt16(Page);
        writer.WriteUInt16(PageSize);
        return {MessageId, writer.TakeBytes()};
    }

    RoomListRequest RoomListRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected room-list request id."); }
        PacketReader reader(packet.Payload);
        RoomListRequest request;
        request.Page = reader.ReadUInt16();
        request.PageSize = reader.ReadUInt16();
        reader.RequireEnd();
        return request;
    }
}
