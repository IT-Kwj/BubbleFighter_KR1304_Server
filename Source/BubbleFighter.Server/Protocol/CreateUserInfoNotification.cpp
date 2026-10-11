#include "CreateUserInfoNotification.h"

#include "PacketReader.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CreateUserInfoNotification::Encode() const { return {MessageId, {}}; }

    CreateUserInfoNotification CreateUserInfoNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected create-user notification message id."); }
        PacketReader{packet.Payload}.RequireEnd();
        return {};
    }
}
