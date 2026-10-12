#include "ServerToClientMessage0x025E.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ServerToClientMessage0x025E::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        return {MessageId, writer.TakeBytes()};
    }

    ServerToClientMessage0x025E ServerToClientMessage0x025E::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid S606 message."); }
        PacketReader reader(packet.Payload);
        ServerToClientMessage0x025E result;
        result.ValueAt0 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
