#include "ClientToServerMessage0x800E.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x800E::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x800E ClientToServerMessage0x800E::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor action 0x800E id."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x800E value{reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}

