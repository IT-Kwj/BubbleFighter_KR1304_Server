#include "MoveIntoResponse.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    bool MoveIntoResponse::IsAccepted() const noexcept
    {
        return AcceptedValue != 0;
    }

    TcpPacket MoveIntoResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(AcceptedValue);
        writer.WriteUInt32(ChannelCategory);
        writer.WriteUInt32(UnknownConnectionValue);
        return {MessageId, writer.TakeBytes()};
    }

    MoveIntoResponse MoveIntoResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected move-into response message id.");
        }
        PacketReader reader(packet.Payload);
        MoveIntoResponse message;
        message.AcceptedValue = reader.ReadUInt32();
        message.ChannelCategory = reader.ReadUInt32();
        message.UnknownConnectionValue = reader.ReadUInt32();
        reader.RequireEnd();
        return message;
    }
}
