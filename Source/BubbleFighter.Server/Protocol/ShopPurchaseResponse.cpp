#include "ShopPurchaseResponse.h"
#include "PacketReader.h"
#include "PacketWriter.h"

#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ShopPurchaseResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(StockId);
        writer.WriteUInt32(Result);
        writer.WriteUtf16(TextAt8);
        writer.WriteUInt8(ByteAt32);
        writer.WriteUInt32(ValueAt36);
        writer.WriteUInt32(ValueAt40);
        writer.WriteUInt32(ValueAt44);
        return {MessageId, writer.TakeBytes()};
    }

    ShopPurchaseResponse ShopPurchaseResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Incorrect shop purchase response message."); }
        PacketReader reader(packet.Payload);
        ShopPurchaseResponse result;
        result.StockId = reader.ReadUInt32();
        result.Result = reader.ReadUInt32();
        result.TextAt8 = reader.ReadUtf16();
        result.ByteAt32 = reader.ReadUInt8();
        result.ValueAt36 = reader.ReadUInt32();
        result.ValueAt40 = reader.ReadUInt32();
        result.ValueAt44 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
