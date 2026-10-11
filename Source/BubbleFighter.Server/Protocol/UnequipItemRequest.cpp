#include "UnequipItemRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket UnequipItemRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt64(ItemIdentifier);
        return {MessageId, writer.TakeBytes()};
    }

    UnequipItemRequest UnequipItemRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Incorrect equipment request message."); }
        PacketReader reader(packet.Payload);
        UnequipItemRequest request{reader.ReadUInt64()};
        reader.RequireEnd();
        return request;
    }
}
