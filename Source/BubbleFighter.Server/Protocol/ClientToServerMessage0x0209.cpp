#include "ClientToServerMessage0x0209.h"
#include "PacketReader.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x0209::Encode() const { return {MessageId, {}}; }

    ClientToServerMessage0x0209 ClientToServerMessage0x0209::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected C521 message ID."); }
        PacketReader(packet.Payload).RequireEnd();
        return {};
    }
}
