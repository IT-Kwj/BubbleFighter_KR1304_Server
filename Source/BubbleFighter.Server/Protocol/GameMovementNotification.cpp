#include "GameMovementNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <bit>
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

    TcpPacket GameMovementNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        for (const auto value : Position) { writer.WriteUInt32(std::bit_cast<std::uint32_t>(value)); }
        writer.WriteUInt8(AngleAt16);
        writer.WriteUInt8(AngleAt17);
        writer.WriteUInt8(FlagsAt18);
        writer.WriteUInt16(ValueAt20);
        return {MessageId, writer.TakeBytes()};
    }

    GameMovementNotification GameMovementNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid full movement message."); }
        PacketReader reader(packet.Payload);
        GameMovementNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        for (auto& value : result.Position) { value = std::bit_cast<float>(reader.ReadUInt32()); }
        result.AngleAt16 = reader.ReadUInt8();
        result.AngleAt17 = reader.ReadUInt8();
        result.FlagsAt18 = reader.ReadUInt8();
        result.ValueAt20 = reader.ReadUInt16();
        reader.RequireEnd();
        return result;
    }
}
