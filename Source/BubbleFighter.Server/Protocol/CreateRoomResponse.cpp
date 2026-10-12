#include "CreateRoomResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CreateRoomResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        writer.WriteUInt32(Result);
        writer.WriteUInt32(DetailValue);
        return {MessageId, writer.TakeBytes()};
    }

    CreateRoomResponse CreateRoomResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected create-room response id."); }
        PacketReader reader(packet.Payload);
        CreateRoomResponse response;
        response.ValueAt0 = reader.ReadUInt32();
        response.Result = reader.ReadUInt32();
        response.DetailValue = reader.ReadUInt32();
        reader.RequireEnd();
        return response;
    }
}
