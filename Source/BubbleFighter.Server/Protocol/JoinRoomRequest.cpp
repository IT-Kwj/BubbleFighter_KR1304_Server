#include "JoinRoomRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket JoinRoomRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(RoomId);
        writer.WriteUtf16(Password);
        return {MessageId, writer.TakeBytes()};
    }

    JoinRoomRequest JoinRoomRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected join-room request id."); }
        PacketReader reader(packet.Payload);
        JoinRoomRequest request;
        request.RoomId = reader.ReadUInt32();
        request.Password = reader.ReadUtf16();
        reader.RequireEnd();
        return request;
    }
}
