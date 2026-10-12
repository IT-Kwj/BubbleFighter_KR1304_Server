#include "CreateRoomRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CreateRoomRequest::Encode() const
    {
        PacketWriter writer;
        Settings.Write(writer);
        writer.WriteUtf16(Password);
        writer.WriteUInt8(TrailingValue ? PresentMarker : AbsentMarker);
        if (TrailingValue) { writer.WriteUInt32(*TrailingValue); }
        return {MessageId, writer.TakeBytes()};
    }

    CreateRoomRequest CreateRoomRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected create-room request id."); }
        PacketReader reader(packet.Payload);
        CreateRoomRequest request;
        request.Settings = SessionDescriptor::Read(reader);
        request.Password = reader.ReadUtf16();
        // 0x0096257A..0x00962585 only produces these two marker values.
        const auto marker = reader.ReadUInt8();
        if (marker == PresentMarker) { request.TrailingValue = reader.ReadUInt32(); }
        else if (marker != AbsentMarker) { throw std::runtime_error("Invalid create-room optional marker."); }
        reader.RequireEnd();
        return request;
    }
}
