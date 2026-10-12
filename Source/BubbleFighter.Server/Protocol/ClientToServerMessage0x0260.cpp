#include "ClientToServerMessage0x0260.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x0260::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt8(EventCode);
        writer.WriteUInt32(ValueAt1);
        writer.WriteUInt32(ValueAt5);
        writer.WriteUInt8(ByteAt9);
        writer.WriteUInt8(ByteAt10);
        writer.WriteUInt8(ByteAt11);
        writer.WriteUInt16(ShortAt12);
        writer.WriteUInt16(ShortAt14);
        return {MessageId, writer.TakeBytes()};
    }

    ClientToServerMessage0x0260 ClientToServerMessage0x0260::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid C608 message id."); }
        PacketReader reader(packet.Payload);
        const ClientToServerMessage0x0260 result{reader.ReadUInt8(),reader.ReadUInt32(),reader.ReadUInt32(),
            reader.ReadUInt8(),reader.ReadUInt8(),reader.ReadUInt8(),reader.ReadUInt16(),reader.ReadUInt16()};
        reader.RequireEnd();
        return result;
    }
}
