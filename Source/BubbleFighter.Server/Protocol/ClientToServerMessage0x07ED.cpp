#include "ClientToServerMessage0x07ED.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x07ED::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt8(EventCode);
        writer.WriteUInt32(ValueAt1);
        writer.WriteUInt8(ByteAt5);
        return {MessageId, writer.TakeBytes()};
    }

    ClientToServerMessage0x07ED ClientToServerMessage0x07ED::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid C2029 message id."); }
        PacketReader reader(packet.Payload);
        const ClientToServerMessage0x07ED result{reader.ReadUInt8(),reader.ReadUInt32(),reader.ReadUInt8()};
        reader.RequireEnd();
        return result;
    }
}
