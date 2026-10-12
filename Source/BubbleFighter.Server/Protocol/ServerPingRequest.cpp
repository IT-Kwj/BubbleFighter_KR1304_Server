#include "ServerPingRequest.h"
#include "PacketReader.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ServerPingRequest::Encode() const { return {MessageId, {}}; }

    ServerPingRequest ServerPingRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected server ping message ID."); }
        PacketReader(packet.Payload).RequireEnd();
        return {};
    }
}
