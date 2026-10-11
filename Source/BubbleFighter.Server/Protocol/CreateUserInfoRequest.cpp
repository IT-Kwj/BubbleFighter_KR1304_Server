#include "CreateUserInfoRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CreateUserInfoRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUtf16(CharacterName);
        writer.WriteUInt32(CharacterType);
        writer.WriteUInt32(ColorIndex);
        writer.WriteUtf16(UnknownString);
        writer.WriteUInt32(UnknownValue);
        return {MessageId, writer.TakeBytes()};
    }

    CreateUserInfoRequest CreateUserInfoRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected create-user request message id.");
        }
        PacketReader reader(packet.Payload);
        CreateUserInfoRequest message;
        message.CharacterName = reader.ReadUtf16();
        message.CharacterType = reader.ReadUInt32();
        message.ColorIndex = reader.ReadUInt32();
        message.UnknownString = reader.ReadUtf16();
        message.UnknownValue = reader.ReadUInt32();
        reader.RequireEnd();
        return message;
    }
}
