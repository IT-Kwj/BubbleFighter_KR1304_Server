#include "GameMovementDeltaNotification.h"
#include "PacketReader.h"
#include "PacketWriter.h"
#include <bit>
#include <limits>
#include <stdexcept>

namespace BubbleFighter::Server::Protocol
{
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);

    TcpPacket GameMovementDeltaNotification::Encode() const
    {
        PacketWriter writer;
        writer.WriteUInt32(ActorObjectId);
        writer.WriteUInt16(ValueAt4);
        writer.WriteUInt8(Mask);
        for (std::size_t i = 0; i < Position.size(); ++i)
        {
            if (((Mask & (1U << i)) != 0) != Position[i].has_value())
            { throw std::invalid_argument("Movement delta position does not match its mask."); }
            if (Position[i]) { writer.WriteUInt32(std::bit_cast<std::uint32_t>(*Position[i])); }
        }
        for (std::size_t i = 0; i < AnglesAndFlags.size(); ++i)
        {
            if (((Mask & (8U << i)) != 0) != AnglesAndFlags[i].has_value())
            { throw std::invalid_argument("Movement delta byte does not match its mask."); }
            if (AnglesAndFlags[i]) { writer.WriteUInt8(*AnglesAndFlags[i]); }
        }
        return {MessageId, writer.TakeBytes()};
    }

    GameMovementDeltaNotification GameMovementDeltaNotification::Decode(const TcpPacket& packet)
    {
        if (packet.MessageId != MessageId) { throw std::invalid_argument("Invalid movement delta message."); }
        PacketReader reader(packet.Payload);
        GameMovementDeltaNotification result;
        result.ActorObjectId = reader.ReadUInt32();
        result.ValueAt4 = reader.ReadUInt16();
        result.Mask = reader.ReadUInt8();
        for (std::size_t i = 0; i < result.Position.size(); ++i)
        { if ((result.Mask & (1U << i)) != 0) { result.Position[i] = std::bit_cast<float>(reader.ReadUInt32()); } }
        for (std::size_t i = 0; i < result.AnglesAndFlags.size(); ++i)
        { if ((result.Mask & (8U << i)) != 0) { result.AnglesAndFlags[i] = reader.ReadUInt8(); } }
        reader.RequireEnd();
        return result;
    }
}
