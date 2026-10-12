#include "CashInventoryPickupRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CashInventoryPickupRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(SlotId);
        return {MessageId, writer.TakeBytes()};
    }
    CashInventoryPickupRequest CashInventoryPickupRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid cash pickup request message."); }
        PacketReader reader(packet.Payload);
        CashInventoryPickupRequest result{reader.ReadUInt32()};
        reader.RequireEnd();
        return result;
    }
}
