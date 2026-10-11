#include "ClientToServerMessage0x0069.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x0069::Encode() const { return {MessageId, {}}; }

    ClientToServerMessage0x0069 ClientToServerMessage0x0069::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || !packet.Payload.empty())
        { throw std::invalid_argument("Invalid C105 empty message."); }
        return {};
    }
}
