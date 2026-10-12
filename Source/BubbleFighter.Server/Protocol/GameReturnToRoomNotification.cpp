#include "GameReturnToRoomNotification.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket GameReturnToRoomNotification::Encode() const { return {MessageId, {}}; }

    GameReturnToRoomNotification GameReturnToRoomNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || !packet.Payload.empty())
        { throw std::invalid_argument("Invalid return-to-room notification."); }
        return {};
    }
}
