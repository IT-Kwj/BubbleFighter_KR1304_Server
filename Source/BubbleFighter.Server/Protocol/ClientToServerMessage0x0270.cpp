#include "ClientToServerMessage0x0270.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x0270::Encode() const
    {
        if (ValueAt4 > 255) { throw std::invalid_argument("C624 second field exceeds native byte domain."); }
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        writer.WriteUInt32(ValueAt4);
        return {MessageId, writer.TakeBytes()};
    }
    ClientToServerMessage0x0270 ClientToServerMessage0x0270::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid C624 message."); }
        PacketReader reader(packet.Payload);
        ClientToServerMessage0x0270 result{reader.ReadUInt32(), reader.ReadUInt32()};
        reader.RequireEnd();
        if (result.ValueAt4 > 255) { throw std::invalid_argument("C624 second field exceeds native byte domain."); }
        return result;
    }
}
