#include "CashBalanceResponse.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket CashBalanceResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ValueAt0);
        writer.WriteUInt32(Cash);
        writer.WriteUInt32(ValueAt8);
        return {MessageId, writer.TakeBytes()};
    }

    CashBalanceResponse CashBalanceResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid cash balance response message."); }
        PacketReader reader(packet.Payload);
        CashBalanceResponse result;
        result.ValueAt0 = reader.ReadUInt32();
        result.Cash = reader.ReadUInt32();
        result.ValueAt8 = reader.ReadUInt32();
        reader.RequireEnd();
        return result;
    }
}
