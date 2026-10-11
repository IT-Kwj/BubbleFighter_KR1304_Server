#include "CreateUserInfoResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CreateUserInfoResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Result);
        return {MessageId, writer.TakeBytes()};
    }

    CreateUserInfoResponse CreateUserInfoResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected create-user response message id.");
        }
        PacketReader reader(packet.Payload);
        CreateUserInfoResponse message;
        message.Result = reader.ReadUInt32();
        reader.RequireEnd();
        return message;
    }
}
