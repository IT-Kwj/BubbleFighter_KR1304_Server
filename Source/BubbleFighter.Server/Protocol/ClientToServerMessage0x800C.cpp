#include "ClientToServerMessage0x800C.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x800C::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt32(ValueAt4);
        writer.WriteUInt8(AxisAt8);
        writer.WriteUInt8(AxisAt9);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x800C ClientToServerMessage0x800C::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid actor action 0x800C id."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x800C value{reader.ReadUInt32(), reader.ReadUInt32(), reader.ReadUInt8(), reader.ReadUInt8()};
        reader.RequireEnd();
        return value;
    }
}

