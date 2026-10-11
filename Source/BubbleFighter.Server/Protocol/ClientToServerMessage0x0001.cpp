#include "ClientToServerMessage0x0001.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x0001::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(InitialValue);
        for (std::size_t index = 0; index < StringFields.size(); ++index)
        {
            if (index == 7)
            {
                writer.WriteUInt32(IntermediateValue);
            }
            writer.WriteUtf16(StringFields[index]);
        }
        return {MessageId, writer.TakeBytes()};
    }

    ClientToServerMessage0x0001 ClientToServerMessage0x0001::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected initial message id.");
        }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x0001 message;
        message.InitialValue = reader.ReadUInt32();
        for (std::size_t index = 0; index < message.StringFields.size(); ++index)
        {
            if (index == 7)
            {
                message.IntermediateValue = reader.ReadUInt32();
            }
            message.StringFields[index] = reader.ReadUtf16();
        }
        reader.RequireEnd();
        return message;
    }
}
