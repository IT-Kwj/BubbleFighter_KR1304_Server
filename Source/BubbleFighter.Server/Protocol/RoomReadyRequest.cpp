#include "RoomReadyRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket RoomReadyRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Ready);
        writer.WriteUInt32(ValueAt4);
        writer.WriteUInt32(ValueAt8);
        return {MessageId, writer.TakeBytes()};
    }

    RoomReadyRequest RoomReadyRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected room-ready request id."); }
        PacketReader reader(packet.Payload);
        RoomReadyRequest request;
        request.Ready = reader.ReadUInt32();
        request.ValueAt4 = reader.ReadUInt32();
        request.ValueAt8 = reader.ReadUInt32();
        reader.RequireEnd();
        // 0x009785E0 writes three zero-extended bytes. Its state-2 branch
        // re-tests 0x00626590 and can then copy a stale byte, so any byte
        // value is possible; consumers treat nonzero as set.
        if (request.Ready > 0xFF || request.ValueAt4 > 0xFF || request.ValueAt8 > 0xFF)
        { throw std::runtime_error("Room-ready request values cannot come from the client writer."); }
        return request;
    }
}
