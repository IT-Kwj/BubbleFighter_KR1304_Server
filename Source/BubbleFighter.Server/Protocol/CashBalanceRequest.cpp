#include "CashBalanceRequest.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CashBalanceRequest::Encode() const { return {MessageId, {}}; }

    CashBalanceRequest CashBalanceRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || !packet.Payload.empty())
        { throw std::invalid_argument("Invalid empty cash balance request."); }
        return {};
    }
}
