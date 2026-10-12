#include "ClientToServerMessage0x802D.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x802D::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x802D ClientToServerMessage0x802D::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor action 0x802D id."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x802D value{reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}

