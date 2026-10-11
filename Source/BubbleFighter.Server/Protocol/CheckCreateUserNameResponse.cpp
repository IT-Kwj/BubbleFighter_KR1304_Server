#include "CheckCreateUserNameResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CheckCreateUserNameResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Result);
        writer.WriteUtf16(CharacterName);
        return {MessageId, writer.TakeBytes()};
    }

    CheckCreateUserNameResponse CheckCreateUserNameResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected name-check response message id.");
        }
        PacketReader reader(packet.Payload);
        CheckCreateUserNameResponse message;
        message.Result = reader.ReadUInt32();
        message.CharacterName = reader.ReadUtf16();
        reader.RequireEnd();
        return message;
    }
}
