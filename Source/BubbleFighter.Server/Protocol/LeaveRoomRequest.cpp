#include "LeaveRoomRequest.h"

#include "PacketReader.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket LeaveRoomRequest::Encode() const { return {MessageId, {}}; }

    LeaveRoomRequest LeaveRoomRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Unexpected leave-room request id."); }
        PacketReader(packet.Payload).RequireEnd();
        return {};
    }
}
