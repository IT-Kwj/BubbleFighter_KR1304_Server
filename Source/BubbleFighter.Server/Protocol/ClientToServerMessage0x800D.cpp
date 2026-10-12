#include "ClientToServerMessage0x800D.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x800D::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(ValueBitsAt4);
        writer.WriteUInt32(ValueAt8);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x800D ClientToServerMessage0x800D::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor action 0x800D id."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x800D value{reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}

