#include "ItemUseRequest.h"
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    TcpPacket ItemUseRequest::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ItemId);
        writer.WriteUInt32(ValueAt4);
        ItemUseArguments::Write(writer, Arguments);
        return {MessageId, writer.TakeBytes()};
    }
    ItemUseRequest ItemUseRequest::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId || packet.Payload.size() > ItemUseArguments::MaxPayloadBytes)
        { throw std::invalid_argument("Invalid item use request."); }
        PacketReader reader(packet.Payload);
        ItemUseRequest value;
        value.ItemId = reader.ReadUInt32(); value.ValueAt4 = reader.ReadUInt32();
        value.Arguments = ItemUseArguments::Read(reader); reader.RequireEnd();
        return value;
    }
}
