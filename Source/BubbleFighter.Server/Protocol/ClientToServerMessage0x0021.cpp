#include "ClientToServerMessage0x0021.h"

#include "PacketReader.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ClientToServerMessage0x0021::Encode() const
    {
        return {MessageId, {}};
    }

    ClientToServerMessage0x0021 ClientToServerMessage0x0021::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        {
            throw std::invalid_argument("Unexpected creation follow-up message id.");
        }
        PacketReader(packet.Payload).RequireEnd();
        return {};
    }
}
