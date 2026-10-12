#include "ItemUseResponse.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ItemUseResponse::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(Result); writer.WriteUInt32(ItemId);
        ItemUseArguments::Write(writer, Arguments);
        return {MessageId, writer.TakeBytes()};
    }
    ItemUseResponse ItemUseResponse::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || packet.Payload.size() > ItemUseArguments::MaxPayloadBytes)
        { throw std::invalid_argument("Invalid item use response."); }
        PacketReader reader(packet.Payload);
        ItemUseResponse value;
        value.Result = reader.ReadUInt32(); value.ItemId = reader.ReadUInt32();
        value.Arguments = ItemUseArguments::Read(reader); reader.RequireEnd();
        return value;
    }
}
