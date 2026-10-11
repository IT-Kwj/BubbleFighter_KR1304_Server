#include "ClientToServerMessage0x006A.h"
#include "PacketReader.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x006A::Encode() const { return {MessageId, {}}; }

    ClientToServerMessage0x006A ClientToServerMessage0x006A::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected plaza request id."); }
        PacketReader(packet.Payload).RequireEnd();
        return {};
    }
}
