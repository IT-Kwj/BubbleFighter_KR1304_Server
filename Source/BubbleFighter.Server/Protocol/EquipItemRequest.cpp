#include "EquipItemRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket EquipItemRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt64(ItemIdentifier);
        return {MessageId, writer.TakeBytes()};
    }

    EquipItemRequest EquipItemRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Incorrect equipment request message."); }
        PacketReader reader(packet.Payload);
        EquipItemRequest request{reader.ReadUInt64()};
        reader.RequireEnd();
        return request;
    }
}
