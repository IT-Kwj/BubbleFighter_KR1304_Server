#include "CashInventoryPickupResponse.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CashInventoryPickupResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(SlotId);
        writer.WriteUInt32(Result);
        writer.WriteUInt32(ValueAt8);
        writer.WriteUtf16(TextAt12);
        return {MessageId, writer.TakeBytes()};
    }
    CashInventoryPickupResponse CashInventoryPickupResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid cash pickup response message."); }
        PacketReader reader(packet.Payload);
        CashInventoryPickupResponse result;
        result.SlotId = reader.ReadUInt32();
        result.Result = reader.ReadUInt32();
        result.ValueAt8 = reader.ReadUInt32();
        result.TextAt12 = reader.ReadUtf16();
        reader.RequireEnd();
        return result;
    }
}
