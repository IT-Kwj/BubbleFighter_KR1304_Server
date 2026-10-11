#include "ShopEntryRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ShopEntryRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt64(CatalogRevision);
        writer.WriteUInt32(SelectedCategory);
        return {MessageId, writer.TakeBytes()};
    }

    ShopEntryRequest ShopEntryRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Incorrect shop entry request message."); }
        PacketReader reader(packet.Payload);
        ShopEntryRequest request;
        request.CatalogRevision = reader.ReadUInt64();
        request.SelectedCategory = reader.ReadUInt32();
        reader.RequireEnd();
        return request;
    }
}
