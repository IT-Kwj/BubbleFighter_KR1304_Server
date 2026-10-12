#include "StartGameRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket StartGameRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Variant);
        writer.WriteUtf16(MapResource);
        writer.WriteUtf16(MapAttribute);
        writer.WriteUtf16(MapKey);
        writer.WriteUInt32(ValueAt76);
        for (const auto value : Rules.Values) { writer.WriteUInt32(value); }
        writer.WriteUInt8(Rules.ByteAt52);
        writer.WriteUInt32(Flags);
        return {MessageId, writer.TakeBytes()};
    }

    StartGameRequest StartGameRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected start-game request id."); }
        PacketReader reader(packet.Payload);
        StartGameRequest request;
        request.Variant = reader.ReadUInt32();
        request.MapResource = reader.ReadUtf16();
        request.MapAttribute = reader.ReadUtf16();
        request.MapKey = reader.ReadUtf16();
        request.ValueAt76 = reader.ReadUInt32();
        for (auto& value : request.Rules.Values) { value = reader.ReadUInt32(); }
        request.Rules.ByteAt52 = reader.ReadUInt8();
        request.Flags = reader.ReadUInt32();
        reader.RequireEnd();
        if (request.Variant > 0xFF || request.Flags > 1)
        { throw std::runtime_error("Start-game request values cannot come from the client writer."); }
        return request;
    }
}
