#include "GamePatchResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GamePatchResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Result);
        writer.WriteUtf16(Message);
        return {MessageId, writer.TakeBytes()};
    }

    GamePatchResponse GamePatchResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected game patch response id.");
        }
        PacketReader reader(packet.Payload);
        GamePatchResponse message;
        message.Result = reader.ReadUInt32();
        message.Message = reader.ReadUtf16();
        reader.RequireEnd();
        return message;
    }
}
