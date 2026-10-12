#include "JoinRoomResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket JoinRoomResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Result);
        writer.WriteUInt32(DetailValue);
        return {MessageId, writer.TakeBytes()};
    }

    JoinRoomResponse JoinRoomResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected join-room response id."); }
        PacketReader reader(packet.Payload);
        JoinRoomResponse response;
        response.Result = reader.ReadUInt32();
        response.DetailValue = reader.ReadUInt32();
        reader.RequireEnd();
        return response;
    }
}
