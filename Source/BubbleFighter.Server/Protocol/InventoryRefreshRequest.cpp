#include "InventoryRefreshRequest.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket InventoryRefreshRequest::Encode() const { return {MessageId, {}}; }

    InventoryRefreshRequest InventoryRefreshRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || !packet.Payload.empty())
        { throw std::invalid_argument("Invalid empty inventory refresh request."); }
        return {};
    }
}
