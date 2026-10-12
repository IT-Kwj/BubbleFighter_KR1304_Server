#include "ClientPingResponse.h"
#include "PacketReader.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientPingResponse::Encode() const { return {MessageId, {}}; }

    ClientPingResponse ClientPingResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected client ping message ID."); }
        PacketReader(packet.Payload).RequireEnd();
        return {};
    }
}
