#include "ClientToServerMessage0x800B.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x800B::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(ValueAt4);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x800B ClientToServerMessage0x800B::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor action 0x800B id."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x800B value{reader.ReadUInt32(), reader.ReadUInt32()};
        reader.RequireEnd();
        return value;
    }
}

