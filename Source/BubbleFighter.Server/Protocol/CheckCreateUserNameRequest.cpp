#include "CheckCreateUserNameRequest.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CheckCreateUserNameRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUtf16(CharacterName);
        return {MessageId, writer.TakeBytes()};
    }

    CheckCreateUserNameRequest CheckCreateUserNameRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected name-check request message id.");
        }
        PacketReader reader(packet.Payload);
        CheckCreateUserNameRequest message;
        message.CharacterName = reader.ReadUtf16();
        reader.RequireEnd();
        return message;
    }
}
