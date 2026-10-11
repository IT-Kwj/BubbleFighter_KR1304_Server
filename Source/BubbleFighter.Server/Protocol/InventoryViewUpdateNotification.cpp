#include "InventoryViewUpdateNotification.h"

#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket InventoryViewUpdateNotification::Encode() const
    {
        PacketWriter writer;
        std::size_t budget = UpdatePartyNotification::MaxTotalEntries;
        View.Write(writer, budget);
        return {MessageId, writer.TakeBytes()};
    }

    InventoryViewUpdateNotification InventoryViewUpdateNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId)
        { throw std::invalid_argument("Unexpected inventory-view update message id."); }
        PacketReader reader(packet.Payload);
        std::size_t budget = UpdatePartyNotification::MaxTotalEntries;
        InventoryViewUpdateNotification message;
        message.View = PartySupplementaryRecord::Read(reader, budget);
        reader.RequireEnd();
        return message;
    }
}
