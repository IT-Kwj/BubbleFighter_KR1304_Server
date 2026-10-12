#include "ClientToServerMessage0x025F.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x025F::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x025F ClientToServerMessage0x025F::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid C607 message id."); }
        PacketReader reader(packet.Payload);
        const ClientToServerMessage0x025F result{reader.ReadUInt32()};
        reader.RequireEnd();
        return result;
    }
}
