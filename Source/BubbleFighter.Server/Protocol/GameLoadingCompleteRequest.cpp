#include "GameLoadingCompleteRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameLoadingCompleteRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(DelayMilliseconds);
        writer.WriteUInt32(ValueAt4);
        writer.WriteUInt32(ValueAt8);
        writer.WriteUInt32(ValueAt12);
        return {MessageId, writer.TakeBytes()};
    }

    GameLoadingCompleteRequest GameLoadingCompleteRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid game loading completion message."); }
        PacketReader reader(packet.Payload);
        GameLoadingCompleteRequest result;
        result.DelayMilliseconds = reader.ReadUInt32();
        result.ValueAt4 = reader.ReadUInt32();
        result.ValueAt8 = reader.ReadUInt32();
        result.ValueAt12 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
