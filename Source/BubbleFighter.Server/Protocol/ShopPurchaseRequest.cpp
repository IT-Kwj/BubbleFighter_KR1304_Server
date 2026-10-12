#include "ShopPurchaseRequest.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ShopPurchaseRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(StockId);
        writer.WriteUInt32(ValueAt4);
        return {MessageId, writer.TakeBytes()};
    }

    ShopPurchaseRequest ShopPurchaseRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Incorrect shop purchase request message."); }
        PacketReader reader(packet.Payload);
        ShopPurchaseRequest result;
        result.StockId = reader.ReadUInt32();
        result.ValueAt4 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
